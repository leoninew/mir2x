"""Read only host-configured credentials. Never return these records to MCP."""

import os
import re
import stat
from pathlib import Path

from .config import Config
from .contract import ContractError, loads, validate
from .errors import BridgeError

MAX_INSTANCE_FILES = 32
if os.name == "nt":
    from pywintypes import error as PlatformError
else:
    PlatformError = OSError


def _no_links(path: Path):
    for part in (path, *path.parents):
        info = part.lstat()
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise OSError("reparse point")


def _windows_read(path: Path) -> bytes:
    import ntsecuritycon
    import win32api
    import win32con
    import win32file
    import win32security

    token = win32security.OpenProcessToken(win32api.GetCurrentProcess(), win32con.TOKEN_QUERY)
    try:
        user = win32security.GetTokenInformation(token, win32security.TokenUser)[0]
    finally:
        token.Close()
    allowed = {win32security.ConvertSidToStringSid(user), "S-1-5-18"}

    def open_checked(target, directory):
        handle = win32file.CreateFile(
            str(target),
            win32con.READ_CONTROL
            | (ntsecuritycon.FILE_READ_ATTRIBUTES if directory else win32con.GENERIC_READ),
            win32con.FILE_SHARE_READ | win32con.FILE_SHARE_WRITE | win32con.FILE_SHARE_DELETE,
            None,
            win32con.OPEN_EXISTING,
            win32file.FILE_FLAG_OPEN_REPARSE_POINT | win32con.FILE_FLAG_BACKUP_SEMANTICS,
            None,
        )
        try:
            info = win32file.GetFileInformationByHandle(handle)
            if info[0] & win32con.FILE_ATTRIBUTE_REPARSE_POINT:
                raise OSError("reparse point")
            if not directory and (info[0] & win32con.FILE_ATTRIBUTE_DIRECTORY or info[7] != 1):
                raise OSError("not regular")
            sd = win32security.GetSecurityInfo(
                handle,
                win32security.SE_FILE_OBJECT,
                win32security.OWNER_SECURITY_INFORMATION | win32security.DACL_SECURITY_INFORMATION,
            )
            acl = sd.GetSecurityDescriptorDacl()
            if sd.GetSecurityDescriptorOwner() != user or acl is None:
                raise OSError("owner or ACL")
            if not sd.GetSecurityDescriptorControl()[0] & win32security.SE_DACL_PROTECTED:
                raise OSError("inherited ACL")
            seen = set()
            for index in range(acl.GetAceCount()):
                header, mask, sid = acl.GetAce(index)
                identity = win32security.ConvertSidToStringSid(sid)
                if header[0] != win32security.ACCESS_ALLOWED_ACE_TYPE or identity not in allowed:
                    raise OSError("unexpected ACL")
                seen.add(identity)
            if seen != allowed:
                raise OSError("missing ACL")
            return handle
        except BaseException:
            handle.Close()
            raise

    directory_handle = open_checked(path.parent, True)
    try:
        file_handle = open_checked(path, False)
        try:
            size = win32file.GetFileSize(file_handle)
            if not 0 < size <= 65536:
                raise OSError("instance size")
            return win32file.ReadFile(file_handle, size)[1]
        finally:
            file_handle.Close()
    finally:
        directory_handle.Close()


def _posix_read(path: Path) -> bytes:
    directory = path.parent.stat()
    if directory.st_uid != os.getuid() or stat.S_IMODE(directory.st_mode) != 0o700:
        raise OSError("directory permissions")
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(descriptor, "rb") as stream:
        info = os.fstat(stream.fileno())
        if (
            info.st_uid != os.getuid()
            or stat.S_IMODE(info.st_mode) != 0o600
            or info.st_nlink != 1
            or not stat.S_ISREG(info.st_mode)
        ):
            raise OSError("file permissions")
        return stream.read(65537)


def read_instances(config: Config, pinned: str | None) -> list[dict]:
    try:
        _no_links(config.bridge_dir)
        wanted = pinned or config.instance_id
        if wanted:
            paths = [config.bridge_dir / f"{wanted}.json"]
        else:
            paths = []
            for entry in config.bridge_dir.iterdir():
                if re.fullmatch("[0-9a-f]{32}\\.json", entry.name):
                    paths.append(entry)
                    if len(paths) > MAX_INSTANCE_FILES:
                        raise BridgeError("ambiguous_instance", "实例文件过多，请配置明确的实例ID")
        instances = []
        for path in sorted(paths):
            _no_links(path)
            data = _windows_read(path) if os.name == "nt" else _posix_read(path)
            if len(data) > 65536:
                raise ContractError("instance_size")
            record = validate(loads(data), "instance")
            if record["instance_id"] != path.stem:
                raise ContractError("instance_identity")
            instances.append(record)
        return instances
    except BridgeError:
        raise
    except FileNotFoundError:
        return []
    except (OSError, PlatformError, ContractError):
        raise BridgeError("unavailable", "实例凭据或权限检查失败") from None
