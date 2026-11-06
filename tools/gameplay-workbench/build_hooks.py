from pathlib import Path

from hatchling.builders.hooks.plugin.interface import BuildHookInterface


class CustomBuildHook(BuildHookInterface):
    def initialize(self, version, build_data):
        # Editable development installs use Vite; distributable wheels carry built assets.
        if self.target_name in {"wheel", "sdist"} and version != "editable":
            assets = Path(self.root) / "web/dist"
            if not (assets / "index.html").is_file():
                raise FileNotFoundError("Run 'corepack yarn build' in web before uv build")
            destination = "mir2x_workbench/static" if self.target_name == "wheel" else "web/dist"
            build_data["force_include"][str(assets)] = destination
