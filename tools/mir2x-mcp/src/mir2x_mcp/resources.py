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
