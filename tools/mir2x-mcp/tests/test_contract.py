import copy
import subprocess
import sys

import pytest
from conftest import FIXTURES, OPERATIONS, ROOT, arguments

from mir2x_mcp.contract import (
    METHODS,
    ContractError,
    StrictValidator,
    input_schema,
    loads,
    output_schema,
    validate,
    validate_arguments,
)


@pytest.mark.parametrize("fixture", FIXTURES, ids=lambda fixture: fixture["method"])
def test_shared_fixtures(fixture):
    method = fixture["method"]
    validate(fixture["request"], method + "Request")
    validate(fixture["success"], method + "Response")
    validate(fixture["failure"], method + "Response")
    validate_arguments(method, arguments(fixture))
    StrictValidator(output_schema(method)).validate(fixture["success"])
    with pytest.raises(ContractError):
        validate(fixture["invalid"], method + "Request")
    with pytest.raises(ContractError):
        validate_arguments(method, arguments(fixture) | {"extra": "forbidden"})


def test_operations():
    assert {operation["state"] for operation in OPERATIONS} == {
        "accepted",
        "running",
        "succeeded",
        "failed",
        "cancelled",
        "unknown",
    }
    for operation in OPERATIONS:
        validate(operation, "operation")


@pytest.mark.parametrize(
    "body",
    [
        b'{"a":1,"a":2}',
        b'{"a":{"b":1,"b":2}}',
        b'{"x":NaN}',
        b'{"x":Infinity}',
        b'{"x":1e999}',
        b'{"a":"\xff"}',
        b'{"a":"\\ud800"}',
        b"{}\x00",
        b"{}{}",
        b"[" * 18 + b"0" + b"]" * 18,
    ],
)
def test_strict_json(body):
    with pytest.raises(ContractError):
        loads(body)


@pytest.mark.parametrize(
    "field,value",
    [
        ("x", True),
        ("x", 1.0),
        ("x", -1),
        ("y", 2147483648),
        ("timeout_ms", 30001),
        ("request_id", "invalid"),
    ],
)
def test_invalid_movement(field, value):
    args = arguments(next(f for f in FIXTURES if f["method"] == "game_move_to"))
    args[field] = value
    with pytest.raises(ContractError):
        validate_arguments("game_move_to", args)


def test_utf8_uint64_and_cast_exclusivity():
    args = arguments(next(f for f in FIXTURES if f["method"] == "game_auction_register"))
    args["note"] = "汉" * 43
    with pytest.raises(ContractError):
        validate_arguments("game_auction_register", args)
    context = copy.deepcopy(args["context"])
    context["world_epoch"] = "18446744073709551616"
    with pytest.raises(ContractError):
        validate(context, "context")
    cast = arguments(next(f for f in FIXTURES if f["method"] == "game_cast"))
    cast.pop("target_handle", None)
    cast.update(x=0, y=0, target_handle="both")
    with pytest.raises(ContractError):
        validate_arguments("game_cast", cast)


def test_generated_files_are_current(tmp_path):
    project = ROOT / "tools/mir2x-mcp"
    files = [
        project / "src/mir2x_mcp" / name
        for name in ("tools.py", "models.py", "contract.schema.json")
    ]
    before = [file.read_bytes() for file in files]
    subprocess.run(
        [sys.executable, str(project / "scripts/generate_tools.py"), "--output-dir", str(tmp_path)],
        check=True,
    )
    assert [(tmp_path / file.name).read_bytes() for file in files] == before
    assert (
        files[-1].read_bytes()
        == (ROOT / "contracts/ai-bridge/v1/contract.schema.json").read_bytes()
    )
    assert len(METHODS) == 35
    for method in METHODS:
        assert input_schema(method)["type"] == "object"
