"""Generated fixed tools. Validate raw arguments before SDK conversion."""

from typing import Annotated, Literal

from mcp_types import CallToolResult, ToolAnnotations
from pydantic import Field

from .models import GameContext, TradeItem


def register_tools(server, bridge):
    @server.tool(
        name="game_get_session",
        description="读取当前实例、角色阶段、控制状态与权限摘要",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_get_session() -> CallToolResult:
        return await bridge.tool("game_get_session", {})

    @server.tool(
        name="game_get_state",
        description="读取当前角色及地图状态",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_get_state() -> CallToolResult:
        return await bridge.tool("game_get_state", {})

    @server.tool(
        name="game_list_nearby",
        description="列出客户端已知的周边对象",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_list_nearby(
        *,
        kind: Literal["all", "player", "monster", "npc", "ground"] = "all",
        radius: Annotated[int, Field(strict=True, ge=0, le=40)] = 15,
        cursor: Annotated[str, Field(strict=True, min_length=1, max_length=128)] = None,
    ) -> CallToolResult:
        return await bridge.tool(
            "game_list_nearby",
            {
                "kind": kind,
                "radius": radius,
                "cursor": cursor,
            },
        )

    @server.tool(
        name="game_get_map_region",
        description="读取当前地图有限区域的地形",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_get_map_region(
        *,
        x: Annotated[int, Field(strict=True, ge=0, le=2147483647)],
        y: Annotated[int, Field(strict=True, ge=0, le=2147483647)],
        width: Annotated[int, Field(strict=True, ge=1, le=33)],
        height: Annotated[int, Field(strict=True, ge=1, le=33)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_get_map_region",
            {
                "x": x,
                "y": y,
                "width": width,
                "height": height,
            },
        )

    @server.tool(
        name="game_get_inventory",
        description="分页读取背包、腰带与装备",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_get_inventory(
        *,
        cursor: Annotated[str, Field(strict=True, min_length=1, max_length=128)] = None,
    ) -> CallToolResult:
        return await bridge.tool(
            "game_get_inventory",
            {
                "cursor": cursor,
            },
        )

    @server.tool(
        name="game_get_skills",
        description="读取已学技能及当前支持的操作",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_get_skills() -> CallToolResult:
        return await bridge.tool("game_get_skills", {})

    @server.tool(
        name="game_get_interaction",
        description="读取当前对话、输入与交易面板",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_get_interaction() -> CallToolResult:
        return await bridge.tool("game_get_interaction", {})

    @server.tool(
        name="game_get_operation",
        description="读取操作进度、结果证据与迟到结论",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_get_operation(
        *,
        operation_id: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_get_operation",
            {
                "operation_id": operation_id,
            },
        )

    @server.tool(
        name="game_get_events",
        description="读取有界事件并报告保留范围缺口",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_get_events(
        *,
        after_seq: Annotated[str, Field(strict=True, pattern="^(0|[1-9][0-9]{0,19})$")],
        limit: Annotated[int, Field(strict=True, ge=1, le=100)] = 100,
    ) -> CallToolResult:
        return await bridge.tool(
            "game_get_events",
            {
                "after_seq": after_seq,
                "limit": limit,
            },
        )

    @server.tool(
        name="game_stop",
        description="优先暂停并停止当前 AI 意图",
        annotations=ToolAnnotations(
            read_only_hint=False, destructive_hint=True, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_stop(
        *,
        operation_id: Annotated[str, Field(strict=True, min_length=1, max_length=128)] = None,
    ) -> CallToolResult:
        return await bridge.tool(
            "game_stop",
            {
                "operation_id": operation_id,
            },
        )

    @server.tool(
        name="game_move_to",
        description="在当前地图使用普通寻路移动；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_move_to(
        *,
        x: Annotated[int, Field(strict=True, ge=0, le=2147483647)],
        y: Annotated[int, Field(strict=True, ge=0, le=2147483647)],
        tolerance: Annotated[int, Field(strict=True, ge=0, le=2)] = 0,
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_move_to",
            {
                "x": x,
                "y": y,
                "tolerance": tolerance,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_npc_open",
        description="打开当前可交互 NPC 对话；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_npc_open(
        *,
        target_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=10000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_npc_open",
            {
                "target_handle": target_handle,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_npc_choose",
        description="选择当前对话的真实选项；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_npc_choose(
        *,
        dialogue_id: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        option_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=10000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_npc_choose",
            {
                "dialogue_id": dialogue_id,
                "option_handle": option_handle,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_npc_input",
        description="提交当前 NPC 输入请求；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_npc_input(
        *,
        input_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        text: Annotated[str, Field(strict=True, min_length=0, max_length=512)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=10000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_npc_input",
            {
                "input_handle": input_handle,
                "text": text,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_npc_close",
        description="关闭当前本地 NPC 对话；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_npc_close(
        *,
        dialogue_id: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=10000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_npc_close",
            {
                "dialogue_id": dialogue_id,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_pick_up",
        description="拾取当前格已观察的地面物品；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_pick_up(
        *,
        ground_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=10000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_pick_up",
            {
                "ground_handle": ground_handle,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_attack",
        description="执行单次或有界持续攻击；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_attack(
        *,
        target_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        mode: Literal["once", "until_dead"] = "once",
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_attack",
            {
                "target_handle": target_handle,
                "mode": mode,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_cast",
        description="使用已学且受支持的技能；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_cast(
        *,
        magic_id: Annotated[int, Field(strict=True, ge=0, le=4294967295)],
        target_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)] = None,
        x: Annotated[int, Field(strict=True, ge=0, le=2147483647)] | None = None,
        y: Annotated[int, Field(strict=True, ge=0, le=2147483647)] | None = None,
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=10000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_cast",
            {
                "magic_id": magic_id,
                "target_handle": target_handle,
                "x": x,
                "y": y,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_use_item",
        description="使用背包中的药品或技能书；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_use_item(
        *,
        item_id: Annotated[int, Field(strict=True, ge=0, le=4294967295)],
        seq_id: Annotated[int, Field(strict=True, ge=0, le=4294967295)],
        count: Annotated[int, Field(strict=True, ge=1, le=65535)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=10000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_use_item",
            {
                "item_id": item_id,
                "seq_id": seq_id,
                "count": count,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_shop_list",
        description="读取当前商店真实商品与报价",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_shop_list(
        *,
        shop_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        item_id: Annotated[int, Field(strict=True, ge=0, le=4294967295)] | None = None,
    ) -> CallToolResult:
        return await bridge.tool(
            "game_shop_list",
            {
                "shop_handle": shop_handle,
                "item_id": item_id,
            },
        )

    @server.tool(
        name="game_shop_buy",
        description="按有效报价与成本上限购买；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_shop_buy(
        *,
        quote_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        count: Annotated[int, Field(strict=True, ge=1, le=2147483647)],
        max_total_gold: Annotated[str, Field(strict=True, pattern="^(0|[1-9][0-9]{0,19})$")],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_shop_buy",
            {
                "quote_handle": quote_handle,
                "count": count,
                "max_total_gold": max_total_gold,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_shop_quote_sell",
        description="请求当前出售界面的物品报价；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_shop_quote_sell(
        *,
        invop_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        item_id: Annotated[int, Field(strict=True, ge=0, le=4294967295)],
        seq_id: Annotated[int, Field(strict=True, ge=0, le=4294967295)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_shop_quote_sell",
            {
                "invop_handle": invop_handle,
                "item_id": item_id,
                "seq_id": seq_id,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_shop_sell",
        description="按有效报价与收入下限出售；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_shop_sell(
        *,
        quote_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        min_total_gold: Annotated[str, Field(strict=True, pattern="^(0|[1-9][0-9]{0,19})$")],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_shop_sell",
            {
                "quote_handle": quote_handle,
                "min_total_gold": min_total_gold,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_trade_request",
        description="向已知玩家申请直接交易；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_trade_request(
        *,
        target_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_trade_request",
            {
                "target_handle": target_handle,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_trade_respond",
        description="处理当前真实交易邀请；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_trade_respond(
        *,
        invitation_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        accept: Annotated[bool, Field(strict=True)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_trade_respond",
            {
                "invitation_handle": invitation_handle,
                "accept": accept,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_trade_offer",
        description="替换当前未锁定的本方报价；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_trade_offer(
        *,
        trade_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        gold: Annotated[str, Field(strict=True, pattern="^(0|[1-9][0-9]{0,19})$")],
        items: Annotated[list[TradeItem], Field(max_length=100)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_trade_offer",
            {
                "trade_handle": trade_handle,
                "gold": gold,
                "items": items,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_trade_lock",
        description="锁定指定版本的本方报价；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_trade_lock(
        *,
        trade_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        offer_revision: Annotated[str, Field(strict=True, pattern="^(0|[1-9][0-9]{0,19})$")],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_trade_lock",
            {
                "trade_handle": trade_handle,
                "offer_revision": offer_revision,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_trade_commit",
        description="确认双方已锁定且版本匹配的交易；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_trade_commit(
        *,
        trade_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        local_revision: Annotated[str, Field(strict=True, pattern="^(0|[1-9][0-9]{0,19})$")],
        peer_revision: Annotated[str, Field(strict=True, pattern="^(0|[1-9][0-9]{0,19})$")],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_trade_commit",
            {
                "trade_handle": trade_handle,
                "local_revision": local_revision,
                "peer_revision": peer_revision,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_trade_cancel",
        description="取消当前直接交易；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_trade_cancel(
        *,
        trade_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_trade_cancel",
            {
                "trade_handle": trade_handle,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_auction_list",
        description="查询或刷新当前拍卖分类",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_auction_list(
        *,
        category: Annotated[int, Field(strict=True, ge=1, le=11)],
        cursor: Annotated[str, Field(strict=True, min_length=1, max_length=128)] = None,
        refresh: Annotated[bool, Field(strict=True)] = False,
    ) -> CallToolResult:
        return await bridge.tool(
            "game_auction_list",
            {
                "category": category,
                "cursor": cursor,
                "refresh": refresh,
            },
        )

    @server.tool(
        name="game_auction_register",
        description="按原有数量语义上架物品；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_auction_register(
        *,
        item_id: Annotated[int, Field(strict=True, ge=0, le=4294967295)],
        seq_id: Annotated[int, Field(strict=True, ge=0, le=4294967295)],
        price: Annotated[str, Field(strict=True, pattern="^[1-9][0-9]{0,18}$")],
        note: Annotated[str, Field(strict=True, min_length=0, max_length=128)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_auction_register",
            {
                "item_id": item_id,
                "seq_id": seq_id,
                "price": price,
                "note": note,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_auction_buy",
        description="按条目与价格上限购买拍卖；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_auction_buy(
        *,
        auction_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        max_total_gold: Annotated[str, Field(strict=True, pattern="^(0|[1-9][0-9]{0,19})$")],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_auction_buy",
            {
                "auction_handle": auction_handle,
                "max_total_gold": max_total_gold,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_auction_cancel",
        description="撤回自己的拍卖条目；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_auction_cancel(
        *,
        auction_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_auction_cancel",
            {
                "auction_handle": auction_handle,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )

    @server.tool(
        name="game_delivery_list",
        description="列出客户端已收到的可信系统投递",
        annotations=ToolAnnotations(
            read_only_hint=True, destructive_hint=False, idempotent_hint=True, open_world_hint=False
        ),
    )
    async def game_delivery_list(
        *,
        cursor: Annotated[str, Field(strict=True, min_length=1, max_length=128)] = None,
    ) -> CallToolResult:
        return await bridge.tool(
            "game_delivery_list",
            {
                "cursor": cursor,
            },
        )

    @server.tool(
        name="game_delivery_claim",
        description="领取指定系统投递并观察回执；返回操作ID后查询进展，unknown不得自动重试。",
        annotations=ToolAnnotations(
            read_only_hint=False,
            destructive_hint=True,
            idempotent_hint=False,
            open_world_hint=False,
        ),
    )
    async def game_delivery_claim(
        *,
        delivery_handle: Annotated[str, Field(strict=True, min_length=1, max_length=128)],
        request_id: Annotated[
            str,
            Field(
                strict=True,
                pattern="^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
            ),
        ],
        context: GameContext,
        timeout_ms: Annotated[int, Field(strict=True, ge=1, le=30000)],
    ) -> CallToolResult:
        return await bridge.tool(
            "game_delivery_claim",
            {
                "delivery_handle": delivery_handle,
                "request_id": request_id,
                "context": context,
                "timeout_ms": timeout_ms,
            },
        )
