"""Generate the versioned bridge contract and deterministic cross-language examples."""
import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
D = {}
def ref(name): return {"$ref": "#/$defs/" + name}
def obj(props, required=None):
    return {"type":"object", "properties":props, "required":list(props) if required is None else required, "additionalProperties":False}
def integer(lo=0, hi=2147483647): return {"type":"integer", "minimum":lo, "maximum":hi}
def string(n=256, minimum=0): return {"type":"string", "minLength":minimum, "maxLength":n, "x-maxUtf8Bytes":n}
def enum(*xs): return {"type":"string", "enum":list(xs)}
def array(item, n=100): return {"type":"array", "items":item, "maxItems":n}
def nullable(s): return {"oneOf":[s,{"type":"null"}]}
def decimal(hi=18446744073709551615):
    return {"type":"string", "pattern":"^(0|[1-9][0-9]{0,19})$", "x-decimalMaximum":hi}
B={"type":"boolean"}
U32=integer(0,4294967295)
COUNT=integer(1,2147483647)
HANDLE=string(128,1)
UUID={"type":"string", "pattern":"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"}
HEX={"type":"string", "pattern":"^[0-9a-f]{32}$"}
D['context']=obj(dict(instance_id=HEX,session_id=HEX,world_epoch=decimal(),control_epoch=decimal()))
D['errorDetails']=obj(dict(server_code=string(64),reason=string(256),retryable=B,side_effects_possible=B),[])
ERROR_CODES='invalid_argument unauthorized version_mismatch ambiguous_instance not_in_game paused stale_context stale_target stale_cursor busy policy_denied budget_exceeded duplicate_conflict ledger_full unreachable stalled timeout transport_lost unsupported_item_type unsupported_skill economic_uncertain out_of_bounds need_move target_lost operation_not_found unavailable server_rejected'.split()
D['error']=obj(dict(code=enum(*ERROR_CODES),message=string(512),details=ref('errorDetails')))
D['evidence']=obj(dict(source=enum('client','server'),kind=enum('server_ack','position','inventory_delta','gold_delta','dialogue','action','target_state','skill','delivery','control'),observed_at=decimal(),snapshot_seq=decimal(),server_code=nullable(string(64)),description=string(512)))
D['operation']=obj(dict(operation_id=HANDLE,request_id=UUID,method=string(64,1),state=enum('accepted','running','succeeded','failed','cancelled','unknown'),context=ref('context'),started_at=decimal(),deadline_at=decimal(),completion_scope=nullable(enum('server_ack','observed_postcondition','client_state')),evidence=array(ref('evidence')),error=nullable(ref('error')),cancel_requested=B,side_effects_possible=B,late_resolution=nullable(obj(dict(state=enum('succeeded','failed'),evidence=array(ref('evidence'))))),result=nullable(ref('actionResult'))))
D['actionResult']=obj(dict(position=ref('position'),dialogue_id=HANDLE,input_handle=HANDLE,trade_handle=HANDLE,quote_handle=HANDLE,quote=ref('quote'),offer_revision=decimal(),local_revision=decimal(),peer_revision=decimal(),partial=B,delivery_pending=B,toggle_enabled=B,item_changes=array(ref('itemDelta')),gold_before=decimal(),gold_after=decimal(),failed_items=array(obj(dict(item_id=U32,seq_id=U32,code=string(64)))),stopped=array(enum('move','attack','follow','queued')),submitted=B),[])
D['itemDelta']=obj(dict(item_id=U32,seq_id=U32,before=U32,after=U32))
D['position']=obj(dict(x=integer(),y=integer()))
D['policySummary']=obj(dict(allowed_actions=array(string(64),37),allow_pvp=B,allow_npc_dialogue=B,max_gold_per_operation=decimal(),max_gold_per_session=decimal(),max_item_count_per_operation=U32,remaining_gold_budget=decimal(),economic_uncertain=B))
D['capabilities']=obj(dict(protocol_version={"const":1},build=string(128),tools=array(string(64),37),supported_skills=array(U32),policy=ref('policySummary'),limitations=array(string(512))))
D['session']=obj(dict(instance_id=HEX,phase=enum('login','select_character','loading','run','disconnected','exiting'),control_state=enum('disabled','read_only','active','paused'),context=nullable(ref('context')),character_uid=nullable(decimal()),pause_reason=nullable(string(128)),capabilities=ref('capabilities')))
D['inventoryItem']=obj(dict(item_id=U32,seq_id=U32,count=U32,name=nullable(string(256)),location=enum('inventory','belt','equipment'),slot=nullable(integer(0,65535)),reserved_count=U32))
D['itemDetails']=obj(dict(item_id=U32,seq_id=nullable(U32),count=nullable(U32),name=nullable(string(256))))
D['groundItem']=obj(dict(item_id=U32,seq_id=nullable(U32),count=nullable(U32),name=nullable(string(256)),location={'const':'ground'}))
D['tradeItem']=obj(dict(item_id=U32,seq_id=U32,count=U32,name=nullable(string(256))))
D['nearbyObject']=obj(dict(target_handle=nullable(HANDLE),ground_handle=nullable(HANDLE),kind=enum('player','monster','npc','ground'),uid=nullable(decimal()),name=nullable(string(256)),position=ref('position'),hp=nullable(integer()),max_hp=nullable(integer()),alive=nullable(B),items=array(ref('groundItem'))))
D['skill']=obj(dict(magic_id=U32,name=string(256),level=integer(0,255),supported=B,action_type=enum('target','ground','self','toggle','unsupported'),cooldown_ms=nullable(integer()),enabled=nullable(B)))
D['option']=obj(dict(option_handle=HANDLE,text=string(2048),closes_dialogue=B))
D['dialogue']=obj(dict(dialogue_id=HANDLE,npc_handle=HANDLE,revision=decimal(),text=string(32768),options=array(ref('option')),source={"const":"game_npc"}))
D['input']=obj(dict(input_handle=HANDLE,prompt=string(2048),max_utf8_bytes=integer(1,512),sensitive=B))
D['quote']=obj(dict(quote_handle=HANDLE,item_id=U32,seq_id=U32,unit_count=U32,gold=decimal(),materials=array(obj(dict(item_id=U32,count=U32))),expires_at=decimal(),direction=enum('buy','sell')))
D['offer']=obj(dict(revision=decimal(),gold=decimal(2147483647),items=array(ref('tradeItem')),locked=B,confirmed=B,pending=B))
D['trade']=obj(dict(trade_handle=HANDLE,peer_uid=decimal(),local=ref('offer'),peer=ref('offer')))
D['auction']=obj(dict(auction_handle=HANDLE,auction_id=decimal(),seller_uid=decimal(),item=ref('itemDetails'),price=decimal(9223372036854775807),note=string(128),expires_at=decimal(),owned=B))
D['delivery']=obj(dict(delivery_handle=HANDLE,source={"const":"system"},auction_id=nullable(decimal()),text=string(2048),claim_state=enum('unclaimed','pending','claimed','unknown'),items=nullable(array(ref('itemDetails'))),gold=nullable(decimal())))
D['interaction']=obj(dict(dialogue=nullable(ref('dialogue')),input=nullable(ref('input')),shop_handle=nullable(HANDLE),invop_handle=nullable(HANDLE),trade=nullable(ref('trade')),invitation=nullable(obj(dict(invitation_handle=HANDLE,peer_uid=decimal()))),auction_open=B,revision=decimal()))
D['event']=obj(dict(event_seq=decimal(),observed_at=decimal(),kind=enum('session','control','snapshot','operation','disconnected','late_resolution'),context=nullable(ref('context')),operation_id=nullable(HANDLE),message=string(512)))

def snap(props):
    return obj(dict(snapshot_seq=decimal(),observed_at=decimal(),source={"const":"client"},coverage=enum('client_known','current_map','current_panel'),unavailable_fields=array(string(128)),**props))
def page(item, **props): return snap(dict(items=array(item),next_cursor=nullable(HANDLE),**props))
D['state']=snap(dict(character_uid=decimal(),name=nullable(string(256)),map_uid=decimal(),map_name=nullable(string(256)),position=ref('position'),action_endpoint=ref('position'),direction=nullable(integer(1,8)),hp=nullable(integer()),max_hp=nullable(integer()),mp=nullable(integer()),max_mp=nullable(integer()),level=nullable(integer()),gold=nullable(decimal()),action=nullable(string(64)),known_states=array(string(128))))
D['nearby']=page(ref('nearbyObject'))
D['inventory']=page(ref('inventoryItem'))
D['skills']=snap(dict(items=array(ref('skill'))))
D['mapRegion']=snap(dict(map_uid=decimal(),x=integer(),y=integer(),width=integer(1,33),height=integer(1,33),cells=array(obj(dict(x=integer(),y=integer(),walkable=B,dynamic_blocked=nullable(B))),1089)))
D['interactionSnapshot']=snap(dict(interaction=ref('interaction')))
D['shopList']=page(ref('quote'),refresh_pending=B,last_query_error=nullable(ref('error')))
D['auctionList']=page(ref('auction'),refresh_pending=B,last_query_error=nullable(ref('error')))
D['deliveryList']=page(ref('delivery'))
D['events']=obj(dict(events=array(ref('event')),gap=B,next_seq=decimal()))
D['stopResult']=obj(dict(paused=B,stopped=array(enum('move','attack','follow','queued')),operation=nullable(ref('operation')),side_effects_possible=B))
D['accepted']=obj(dict(operation_id=HANDLE,state=enum('accepted','running','succeeded','failed','cancelled','unknown')))

METHODS={}
def method(name, params, result, action=False, timeout=10000):
    name='game_'+name
    METHODS[name]={'action':action,'params':params,'result':ref(result),'timeout_ms':timeout if action else None}
empty=obj({})
cursor=obj(dict(cursor=HANDLE),[])
method('get_session',empty,'session')
method('get_state',empty,'state')
method('list_nearby',obj(dict(kind=enum('all','player','monster','npc','ground'),radius=integer(0,40),cursor=HANDLE),[]),'nearby')
method('get_map_region',obj(dict(x=integer(),y=integer(),width=integer(1,33),height=integer(1,33))),'mapRegion')
method('get_inventory',cursor,'inventory')
method('get_skills',empty,'skills')
method('get_interaction',empty,'interactionSnapshot')
method('get_operation',obj(dict(operation_id=HANDLE)),'operation')
method('get_events',obj(dict(after_seq=decimal(),limit=integer(1,100)),['after_seq']),'events')
method('stop',{'oneOf':[obj(dict(operation_id=HANDLE,interrupt_only={**B,'const':False}),[]),obj(dict(interrupt_only={**B,'const':True},operation_id=HANDLE,context=ref('context')))]},'stopResult')
method('move_to',obj(dict(x=integer(),y=integer(),tolerance=integer(0,2)),['x','y']),'accepted',True,30000)
method('npc_open',obj(dict(target_handle=HANDLE)),'accepted',True)
method('npc_choose',obj(dict(dialogue_id=HANDLE,option_handle=HANDLE)),'accepted',True)
method('npc_input',obj(dict(input_handle=HANDLE,text=string(512))),'accepted',True)
method('npc_close',obj(dict(dialogue_id=HANDLE)),'accepted',True)
method('pick_up',obj(dict(ground_handle=HANDLE)),'accepted',True)
method('attack',obj(dict(target_handle=HANDLE,mode=enum('once','until_dead')),['target_handle']),'accepted',True,30000)
cast={'oneOf':[obj(dict(magic_id=U32,target_handle=HANDLE)),obj(dict(magic_id=U32,x=integer(),y=integer())),obj(dict(magic_id=U32))]}
method('cast',cast,'accepted',True)
method('use_item',obj(dict(item_id=U32,seq_id=U32,count=integer(1,65535))),'accepted',True)
method('shop_list',obj(dict(shop_handle=HANDLE,item_id=U32),['shop_handle']),'shopList')
method('shop_buy',obj(dict(quote_handle=HANDLE,count=COUNT,max_total_gold=decimal())),'accepted',True,30000)
method('shop_quote_sell',obj(dict(invop_handle=HANDLE,item_id=U32,seq_id=U32)),'accepted',True,30000)
method('shop_sell',obj(dict(quote_handle=HANDLE,min_total_gold=decimal())),'accepted',True,30000)
method('trade_request',obj(dict(target_handle=HANDLE)),'accepted',True,30000)
method('trade_respond',obj(dict(invitation_handle=HANDLE,accept=B)),'accepted',True,30000)
method('trade_offer',obj(dict(trade_handle=HANDLE,gold=decimal(2147483647),items=array(obj(dict(item_id=U32,seq_id=U32,count=COUNT))))),'accepted',True,30000)
method('trade_lock',obj(dict(trade_handle=HANDLE,offer_revision=decimal())),'accepted',True,30000)
method('trade_commit',obj(dict(trade_handle=HANDLE,local_revision=decimal(),peer_revision=decimal())),'accepted',True,30000)
method('trade_cancel',obj(dict(trade_handle=HANDLE)),'accepted',True,30000)
method('auction_list',obj(dict(category=integer(1,11),cursor=HANDLE,refresh=B),['category']),'auctionList')
method('auction_register',obj(dict(item_id=U32,seq_id=U32,price={**decimal(9223372036854775807),'pattern':'^[1-9][0-9]{0,18}$'},note=string(128))),'accepted',True,30000)
method('auction_buy',obj(dict(auction_handle=HANDLE,max_total_gold=decimal())),'accepted',True,30000)
method('auction_cancel',obj(dict(auction_handle=HANDLE)),'accepted',True,30000)
method('delivery_list',cursor,'deliveryList')
method('delivery_claim',obj(dict(delivery_handle=HANDLE)),'accepted',True,30000)
D['grantResult']=obj(dict(grant_id=UUID,mode=enum('new_task','continue_task'),control_state={'const':'active'},context=ref('context')))
method('grant_control',obj(dict(mode=enum('new_task','continue_task'),grant_id=UUID,request_id=UUID,character_uid=decimal(),context=ref('context'))),'grantResult')
method('shop_close',obj(dict(shop_handle=HANDLE)),'accepted',True,5000)
assert len(METHODS)==37
for name, meta in METHODS.items():
    meta['write'] = meta['action'] or name in ('game_stop','game_grant_control')


# Distinct terminal shapes prevent a successful operation from omitting evidence.
operation_base=D['operation']
operation_branches=[]
for state in ['accepted','running','succeeded','failed','cancelled','unknown']:
    branch=copy.deepcopy(operation_base)
    branch['properties']['state']={'const':state}
    if state=='succeeded':
        branch['properties']['completion_scope']=enum('server_ack','observed_postcondition','client_state')
        branch['properties']['evidence']['minItems']=1
    else:
        branch['properties']['completion_scope']={'type':'null'}
    if state=='failed':branch['properties']['error']=ref('error')
    operation_branches.append(branch)
D['operation']={'oneOf':operation_branches}

DESCRIPTIONS=[
'读取当前实例、角色阶段、控制状态与权限摘要','读取当前角色及地图状态','列出客户端已知的周边对象','读取当前地图有限区域的地形','分页读取背包、腰带与装备','读取已学技能及当前支持的操作','读取当前对话、输入与交易面板','读取操作进度、结果证据与迟到结论','读取有界事件并报告保留范围缺口','明确停止撤销授权；interrupt_only须精确操作及context，仅中断该操作并保留控制状态',
'在当前地图使用普通寻路移动','打开当前可交互 NPC 对话','选择当前对话的真实选项','提交当前 NPC 输入请求','关闭当前本地 NPC 对话','拾取当前格已观察的地面物品','执行单次或有界持续攻击','使用已学且受支持的技能','使用背包中的药品或技能书','读取当前商店真实商品与报价','按有效报价与成本上限购买','请求当前出售界面的物品报价','按有效报价与收入下限出售','向已知玩家申请直接交易','处理当前真实交易邀请','替换当前未锁定的本方报价','锁定指定版本的本方报价','确认双方已锁定且版本匹配的交易','取消当前直接交易','查询或刷新当前拍卖分类','按原有数量语义上架物品','按条目与价格上限购买拍卖','撤回自己的拍卖条目','列出客户端已收到的可信系统投递','领取指定系统投递并观察回执']
DESCRIPTIONS += ['新明确用户任务授权，或同任务在重新观察后续用；旧goal、轮询和重试不得创建新授权，响应丢失不得重发', '关闭当前本地商店面板，不关闭NPC对话、不撤销已提交购买，无网络发送']
for (name,m),description in zip(METHODS.items(), DESCRIPTIONS,strict=True):
    m['description']=description
for name,field,default in [('game_move_to','tolerance',0),('game_attack','mode','once'),('game_auction_list','refresh',False),('game_list_nearby','kind','all'),('game_list_nearby','radius',15),('game_get_events','limit',100)]:
    METHODS[name]['params']['properties'][field]['default']=default

base=dict(v={'const':1},id=string(64,1))
for name,m in METHODS.items():
    props=dict(base,method={'const':name},params=m['params'])
    if m['action']:
        props.update(request_id=UUID,context=ref('context'),timeout_ms=integer(1,m['timeout_ms']))
    D[name+'Request']=obj(props)
    success=obj(dict(base,ok={'const':True},data=m['result'],context=nullable(ref('context'))))
    # The response id is the request id; version/id are still mandatory.
    failure=obj(dict(base,ok={'const':False},error=ref('error'),context=nullable(ref('context'))))
    D[name+'Response']={'oneOf':[success,failure]}
D['helloRequest']=obj(dict(base,method={'const':'hello'},params=obj(dict(instance_id=HEX,token={'type':'string','pattern':'^[0-9a-f]{64}$'}))))
D['heartbeatRequest']=obj(dict(base,method={'const':'heartbeat'},params=empty))
D['transportError']=obj(dict(v={'const':1},id=nullable(string(64,1)),ok={'const':False},error=ref('error'),context=nullable(ref('context'))))
D['helloResponse']=obj(dict(base,ok={'const':True},data=obj(dict(instance_id=HEX,protocol_version={'const':1})),context={'type':'null'}))
D['heartbeatResponse']=obj(dict(base,ok={'const':True},data=obj(dict(alive={'const':True})),context={'type':'null'}))
action_names=[k for k,v in METHODS.items() if v['write']]
allow_actions={'oneOf':[{'const':'*'},{'type':'array','items':enum(*action_names),'maxItems':37,'uniqueItems':True}]}
def allow(s): return {'oneOf':[{'const':'*'},{'type':'array','items':s,'maxItems':4096,'uniqueItems':True}]}
D['policy']=obj(dict(allowed_actions=allow_actions,allow_pvp=B,allowed_peer_uids=allow(decimal()),allowed_item_ids=allow(U32),max_gold_per_operation=decimal(),max_gold_per_session=decimal(),max_item_count_per_operation=U32,allow_npc_dialogue=B),[])
D['instance']=obj(dict(v={'const':1},instance_id=HEX,token={'type':'string','pattern':'^[0-9a-f]{64}$'},pid=decimal(),started_at=decimal(),port=integer(1,65535),build=string(128,1)))
D['request']={'oneOf':[ref(name+'Request') for name in METHODS]+[ref('helloRequest'),ref('heartbeatRequest')]}
D['snapshot']={'anyOf':[ref('state'),ref('nearby'),ref('inventory'),ref('skills'),ref('mapRegion'),ref('interactionSnapshot')]}

contract={'$schema':'https://json-schema.org/draft/2020-12/schema','$id':'https://mir2x.local/contracts/ai-bridge/v1/contract.schema.json','$ref':'#/$defs/request','title':'mir2x 当前角色桥接 v1','description':'封闭对象；x-maxUtf8Bytes 和 x-decimalMaximum 是跨语言必须执行的附加约束。','$defs':D,'x-methods':METHODS}
(ROOT/'contract.schema.json').write_text(json.dumps(contract,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def sample(s):
    if '$ref' in s: return sample(D[s['$ref'].rsplit('/',1)[1]])
    if 'const' in s:return s['const']
    if 'enum' in s:return s['enum'][0]
    if 'oneOf' in s:
        branch=next((b for b in s['oneOf'] if b.get('type')=='null'),s['oneOf'][0])
        return sample(branch)
    t=s.get('type')
    if t=='object':return {k:sample(s['properties'][k]) for k in s['required']}
    if t=='array':return []
    if t=='integer':return s.get('minimum',0)
    if t=='boolean':return False
    if t=='null':return None
    if t=='string':
        pat=s.get('pattern','')
        if '{64}' in pat:return '0'*64
        if '{32}' in pat:return '0'*32
        if '{8}' in pat:return '00000000-0000-4000-8000-000000000001'
        if pat.startswith('^[1-9]'):return '1'
        if 'x-decimalMaximum' in s:return '0'
        return 'example' if s.get('minLength',0) else ''
    raise ValueError(s)

fixtures=[]
for name,m in METHODS.items():
    request=sample(D[name+'Request'])
    success=sample(D[name+'Response']['oneOf'][0]); success['id']=request['id']
    failure=sample(D[name+'Response']['oneOf'][1]);failure['error']={'code':'not_in_game','message':'角色尚未进入游戏','details':{}}
    invalid=copy.deepcopy(request);invalid['params']['unexpected']=True
    fixtures.append(dict(method=name,request=request,success=success,failure=failure,invalid=invalid))
(ROOT/'fixtures.json').write_text(json.dumps(fixtures,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
outcomes=[]
for state in ['accepted','running','succeeded','failed','cancelled','unknown']:
    op=sample(next(b for b in operation_branches if b['properties']['state']['const']==state));op.update(state=state,method='game_move_to')
    if state=='succeeded':
        op['completion_scope']='observed_postcondition';op['evidence']=[sample(ref('evidence'))]
    if state=='failed':op['error']={'code':'unreachable','message':'目标不可达','details':{}}
    if state=='unknown':op['side_effects_possible']=True
    outcomes.append(op)
(ROOT/'operation-fixtures.json').write_text(json.dumps(outcomes,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(ROOT/'policy-readonly.json').write_text('{}\n',encoding='utf-8')
policy=dict(allowed_actions=action_names,allow_pvp=False,allowed_peer_uids=[],allowed_item_ids='*',max_gold_per_operation='1000',max_gold_per_session='10000',max_item_count_per_operation=10,allow_npc_dialogue=True)
(ROOT/'policy-example.json').write_text(json.dumps(policy,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


errors=[dict(v=1,id='example',ok=False,error=dict(code=c,message='操作未完成，请按错误码处理',details={}),context=None) for c in ERROR_CODES]
(ROOT/'error-fixtures.json').write_text(json.dumps(errors,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
