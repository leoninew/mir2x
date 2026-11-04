from importlib.resources import files

from .contract import dumps
from .errors import BridgeError

RESOURCES = {
    "session": ("game_get_session", "当前实例、角色阶段及控制权"),
    "character": ("game_get_state", "当前角色状态；未登录时返回明确错误"),
    "nearby": ("game_list_nearby", "客户端已知周边第一页；更多内容使用查询工具"),
    "inventory": ("game_get_inventory", "背包与装备第一页；更多内容使用查询工具"),
    "interaction": ("game_get_interaction", "当前NPC、输入及交易面板"),
    "capabilities": ("game_get_session", "真实客户端能力与冻结策略；不会更改授权"),
}

KNOWLEDGE_RESOURCES = {
    "leveling": ("leveling.md", "练级参考索引；结合实时角色选择地图、装备和技能阶段"),
    "leveling/maps": ("leveling-maps.md", "练级地图、自然怪物与经验归属的源码参考"),
    "leveling/equipment": ("leveling-equipment.md", "分阶段装备、普通商店与穿戴确认参考"),
    "leveling/skills": ("leveling-skills.md", "技能条件、正常学习路径与经验曲线参考"),
}


def register_resources(server, bridge):
    def register(name, method, description):
        @server.resource(
            f"mir2x://{name}", name=name, description=description, mime_type="application/json"
        )
        async def read() -> str:
            try:
                response = await bridge.call(method, {})
                if name == "capabilities" and response["ok"]:
                    response = dict(response, data=response["data"]["capabilities"])
            except BridgeError as exc:
                response = exc.payload()
            return dumps(response).decode("utf-8")

    for name, (method, description) in RESOURCES.items():
        register(name, method, description)

    def register_knowledge(path, filename, description):
        @server.resource(
            f"mir2x://knowledge/{path}",
            name=path.replace("/", "-"),
            description=description,
            mime_type="text/markdown",
        )
        async def read() -> str:
            return files("mir2x_mcp").joinpath("runtime_assets", filename).read_text("utf-8")

    for path, (filename, description) in KNOWLEDGE_RESOURCES.items():
        register_knowledge(path, filename, description)
