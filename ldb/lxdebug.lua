--[[

Copyright (C) lcinx
lcinx@163.com

]]


module(...,package.seeall)
require "lxnet"

local _G = _G
local pairs = pairs
local assert = assert
local type = type
local next = next
local tostring = tostring
local tonumber = tonumber
local slen = string.len
local sformat = string.format
local ssub = string.sub
local sfind = string.find
local table = table
local tinsert = table.insert
local tconcat = table.concat
local srep = string.rep
local io = io
local dtraceback = debug.traceback
local dsethook = debug.sethook
local dgethook = debug.gethook
local dgetinfo = debug.getinfo
local dgetlocal = debug.getlocal

local lxnet = lxnet
local socketer = socketer
local listener = listener
local packet = packet
local delay = delay
local getfilefullname = getfilefullname

--记录下本文件的文件名，防止调试本文件
local s_thisfilename = dgetinfo(1, "S").source

--使用树形或者大括号的方式进行输出；默认大括号方式
local s_usedtree = false

--管理网络部分
local s_netmgr = {
	port = nil,						--监听端口
	listen = nil,					--用于监听的对象
	client = nil,					--接受的宿主连接，只可接受一个
	needrelease = false				--暂时未用，记录下是否是从脚本里初始化网络模块的。
}

--runmodule 模式： 纯脚本: purelua  使用帧函数: useframefunc

--管理调试信息
local s_debugmgr = {
	runmodule = "unknow",			--运行模式。纯脚本、以及执行debug_runonce的嵌入，可高速模式。
	lastcmd = "",					--上次执行的命令
	lastnum = 1,					--若上次是l命令，则此值就是上次获取的最后的行数。

	num = 1,						--断点编号
	trace = false,					--调试状态，当为true时，则为step info模式。
	nextstep = false,				--单步调试
	current_func = nil,				--当前运行的函数
	trace_count = 0,				--当前调用堆栈的深度
	breaktable = {					--断点集
		num = 0,					--断点总数
		validnum = 0,				--有效断点数目
		tb = {}						--断点表[line][source]
	}	
}

local function realaddbreakpoint(line, source)
	if s_debugmgr.breaktable.tb[line] and s_debugmgr.breaktable.tb[line][source] then
		local tl = s_debugmgr.breaktable.tb[line][source]
		if tl.active then
			return "Note: breakpoint "..tostring(tl.number).." already at file: "..source..", line "..tostring(line).."."
		else
			s_debugmgr.breaktable.validnum = s_debugmgr.breaktable.validnum + 1
			tl.active = true
			return "Enable breakpoint "..tostring(tl.number).."."
		end
	end
	local tbl = {}
	tbl.source = source
	tbl.line = line
	tbl.active = true
	tbl.number = s_debugmgr.num

	if not s_debugmgr.breaktable.tb[line] then
		s_debugmgr.breaktable.tb[line] = {}
	end

	local oldvalidnum = s_debugmgr.breaktable.validnum

	s_debugmgr.breaktable.tb[line][source] = tbl
	s_debugmgr.breaktable.num = s_debugmgr.breaktable.num + 1
	s_debugmgr.breaktable.validnum = s_debugmgr.breaktable.validnum + 1

	--若激活的数目从0变为大于0，则尝试下变更模式
	if oldvalidnum == 0 then
		userlinetrace()
	end

	s_debugmgr.num = s_debugmgr.num + 1

	return "Breakpoint "..tostring(tbl.number).." at file: "..source..", line "..tostring(line).."."
end

--增加断点
local function addbreakpoint(line, source)
	--检查文件是否存在，检查此行是否存在
	source = getfilefullname(source)
	local f = io.open(source, "r")
	if not f then
		return "No source file named "..source.."."
	end

	local count = 0
	for l in f:lines() do
		count = count + 1
		if count == line then
			break
		end
	end
	f:close()

	--没找到这个行，
	if count ~= line then
		return 'No line '..tostring(line)..' in file "'..source..'"'
	end

	return realaddbreakpoint(line, source)
end

--根据断点id查找断点
local function findbreakpoint(bpnum)
	local s1 = s_debugmgr.breaktable.tb
	for k, v in pairs(s1) do
		for i, j in pairs(v) do
			if j.number == bpnum then
				return j
			end
		end
	end
	return nil
end

--删除断点
local function delbreakpoint(bpnum)
	if bpnum == "*" then
		local oldnum = s_debugmgr.breaktable.num
		s_debugmgr.breaktable.num = 0
		s_debugmgr.breaktable.validnum = 0
		s_debugmgr.breaktable.tb = {}

		if  oldnum ~= 0 then
			niltrace()
		end
		return "Delete all breakpoints."
	end
	local bp = findbreakpoint(tonumber(bpnum))
	if not bp then
		return "No breakpoint number "..bpnum.."."
	end
	s_debugmgr.breaktable.tb[bp.line][bp.source] = nil

	s_debugmgr.breaktable.num = s_debugmgr.breaktable.num - 1
	if bp.active then
		s_debugmgr.breaktable.validnum = s_debugmgr.breaktable.validnum - 1
	end
	if (s_debugmgr.breaktable.num == 0) or (s_debugmgr.breaktable.validnum == 0) then
		niltrace()
	end
	return "Delete breakpoint "..bpnum.."."
end

--激活指定的断点
local function enablebreakpoint(bpnum)
	if s_debugmgr.breaktable.num <= 0 then
		return "No breakpoints."
	end
	--激活所有
	if bpnum == "*" then
		local oldvalidnum = s_debugmgr.breaktable.validnum
		s_debugmgr.breaktable.validnum = s_debugmgr.breaktable.num
	
		local s1 = s_debugmgr.breaktable.tb
		for k, v in pairs(s1) do
			for i, j in pairs(v) do
				j.active = true
			end
		end
		--若激活的数目从0变为大于0，则尝试下变更模式
		if oldvalidnum == 0 then
			userlinetrace()
		end
		return "Enable all breakpoints."
	end

	local bp = findbreakpoint(tonumber(bpnum))
	if not bp then
		return "No breakpoint number "..bpnum.."."
	end

	if not bp.active then
		local oldvalidnum = s_debugmgr.breaktable.validnum
		s_debugmgr.breaktable.validnum = s_debugmgr.breaktable.validnum + 1
		bp.active = true

		--当之前被激活的数目为0，则此时为大于0了，那么转换模式
		if oldvalidnum == 0 then
			userlinetrace()
		end
	end
	return "Enable breakpoint "..bpnum.."."
end

--禁用指定的断点
local function disablebreakpoint(bpnum)
	if s_debugmgr.breaktable.num <= 0 then
		return "No breakpoints."
	end
	--禁用所有
	if bpnum == "*" then
		local oldvalidnum = s_debugmgr.breaktable.validnum
		s_debugmgr.breaktable.validnum = 0
	
		local s1 = s_debugmgr.breaktable.tb
		for k, v in pairs(s1) do
			for i, j in pairs(v) do
				j.active = false
			end
		end

		if oldvalidnum > 0 then
			niltrace()
		end
		return "Disable all breakpoints."
	end

	local bp = findbreakpoint(tonumber(bpnum))
	if not bp then
		return "No breakpoint number "..bpnum.."."
	end

	if bp.active then
		s_debugmgr.breaktable.validnum = s_debugmgr.breaktable.validnum - 1
		bp.active = false
		if s_debugmgr.breaktable.validnum == 0 then
			niltrace()
		end
	end
	return "Disable breakpoint "..bpnum.."."
end

local function sendmsg(msg)
	socketer.sendmsg(s_netmgr.client, msg)
	socketer.realsend(s_netmgr.client)
end

--得到调用堆栈的深度
local function get_functionframe_deep()
	local level = 1
	local info
	while true do
		info = dgetinfo(level, "S")
		if not info then
			return level
		end
		level = level + 1
	end
end

--获取指定文件从某行开始，获取指定行数
local function getfileline (filename, beginline, linecount)
	local linenum = 0
	local count = 0
	local retlines = {}
	local srcline
	local f = io.open(filename, "r")
	if not f then
		tinsert(retlines, "open ".. filename.." failed!")
		linenum = 1
		return retlines, linenum
	end
	beginline = (beginline >= 1 and beginline) or 1
	linecount = (linecount >= 1 and linecount) or 1

	for line in f:lines() do
		count = count + 1
		if count >= beginline and count < (beginline + linecount) then
			srcline = sformat("%-6d %s", count, line)
			tinsert(retlines, srcline)
			linenum = linenum + 1
		end
	end
	f:close()
	return retlines, linenum
end

--发送当前断点或开始位置的源代码列表
local function sendlist_source()
	local env = dgetinfo(5)
	local begin = 1
	if env.currentline > 5 then
		begin = env.currentline - 5
	end

	--若上次命令为l，则从上次的位置开始，否则，设置开始位置为这次的开始
	if s_debugmgr.lastcmd == "l" then
		begin = s_debugmgr.lastnum
	else
		s_debugmgr.lastnum = begin
	end
	local filename = ssub(env.source, 2, #env.source)
	local retlines, num = getfileline(filename, begin, 10)

	--不论上次是否为l命令，都变更下下次开始位置。
	s_debugmgr.lastnum = s_debugmgr.lastnum + num

	if num == 0 then
		local src = "Line number "..begin.." out of range; "..filename.." has "..(begin - 1).." lines."
		tinsert(retlines, src)
		num = 1
	end
	local msg = packet.anyfor_ldb()
	packet.settype(msg, 0)
	packet.pushint16(msg, num)
	for k, v in pairs(retlines) do
		packet.pushstring(msg, v .. '\n')
	end
	sendmsg(msg)
end

--发送调用栈信息
local function send_tracebackinfo()
	local msg = packet.anyfor_ldb()
	packet.settype(msg, 0)
	packet.pushint16(msg, 1)
	packet.pushstring(msg, dtraceback("", 5) .. '\n')
	sendmsg(msg)
end

--发送断点列表信息
local function sendbreakpointlist()
	local msg = packet.anyfor_ldb()
	local num = 1
	packet.settype(msg, 0)
	packet.pushint16(msg, num)
	if s_debugmgr.breaktable.num == 0 then
		packet.pushstring(msg, "No breakpoints.\n")
	else
		packet.pushstring(msg, "Num     Enb   What\n")
		local s = s_debugmgr.breaktable.tb
		local state
		for k, v in pairs(s) do
			for i, j in pairs(v) do
				if j.active then
					state = "y"
				else
					state = "n"
				end
				packet.pushstring(msg, sformat("%-8d%-6s%s:%d", j.number, state, j.source, j.line) ..'\n')
				num = num + 1
			end
		end
	end
	packet.pushint16toindex(msg, 0, num)
	sendmsg(msg)
end

local s_num = 0
local s_msg = nil

--真实的装入
local function realpushstring(str)
	local strlen = slen(str)
	if not packet.canpush(s_msg, strlen + 4) then
		packet.settype(s_msg, 1)
		packet.pushint16toindex(s_msg, 0, s_num)
		sendmsg(s_msg)
		
		s_num = 0
		packet.reset(s_msg)
		packet.settype(s_msg, 0)
		packet.pushint16(s_msg, s_num)
	end
	
	packet.pushstring(s_msg, str)
	s_num = s_num + 1
end

--装入字符串
local function localpushstring(str)
	str = str .. '\n'
	local strlen = slen(str)
	local delay = 32750
	if strlen > delay then
		--看需要分拆成几段
		local begin = 0
		local endidx = delay
		local second
		while true do
			second = ssub(str, begin, endidx)
			realpushstring(second)
			if endidx + delay >= strlen then
				break
			end
			begin = endidx + 1
			endidx = endidx + delay
		end

		str = ssub(str, endidx + 1)
	end
	realpushstring(str)
end

--表格型输出表
local deep_table = {}
local function debug_print_var(name, value, level, pnum, fullname)
	local prefix = srep("    ", level)
	local str = sformat("%s%s = %s    [type:%s]", prefix, name, tostring(value), type(value))

	if type(value) == "table" and pnum ~= 0 then
		
		--加到临时表中,以免表出现循环引用时,打印也产生死循环
		if not deep_table[value] then
			deep_table[value] = " = {" ..fullname .. "} [type:reference]"
		else
			localpushstring(prefix..''..name..deep_table[value])
			return
		end
			
		--打印表中所有数据
		localpushstring(sformat("%s%s = \n%s{", prefix, name, prefix))
		pnum = pnum - 1
		for k, v in pairs (value) do	
			debug_print_var(k, v, level + 1, pnum, fullname.."."..k)
		end
		localpushstring(prefix .. "}")
	else
		localpushstring(str)
	end
end

--树形输出表
local function print_r(name, root, flag)
	local cache = {  [root] = name }
	local function _dump(t,space,name)
		local temp = {}
		for k,v in pairs(t) do
			local key = tostring(k)
			if cache[v] then
				tinsert(temp,"+" .. key .. " {" .. cache[v].."} [type:reference]")
			elseif type(v) == "table" then
				local new_key = name .. "." .. key
				cache[v] = new_key
				if flag ~= 1 then
					tinsert(temp,"+" .. key .. _dump(v,space .. (next(t,k) and "|" or " " ).. srep(" ",#key),new_key))
				else
					tinsert(temp,"+" .. key .. " = " .. tostring(v).. " [type:".. type(v).."]")
				end
			else
				tinsert(temp,"+" .. key .. " = " .. tostring(v).. " [type:".. type(v).."]")
			end
		end
		return tconcat(temp,"\n"..space)
	end
	localpushstring(name.._dump(root, srep(" ", #name),name))
end

--循环解析变量名，并找出要输出的变量
local function loop_print_var(var, value, pnum)

	local find 		= false
	local begin_pos = 1
	local end_pos	= 1	
	local len 		= slen(var)

	local name
	local dep_num = 0
	while end_pos < len do
	
		begin_pos, end_pos = sfind(var,"[_%w][_%w]*",end_pos)
	
		if not begin_pos then
			break
		end
	
		name = ssub(var,begin_pos, end_pos)
		end_pos = end_pos + 1
		
		--如果第一个字符为数字，则变为整数，而不是字符串
		if sfind(name, "%d") == 1 then
			name = tonumber(name)
		end
		if not value[name] then
			if dep_num == 1 then
				return false
			end
		else
			if type(value[name]) == "table" then
				if end_pos >= len then
					if s_usedtree then
						print_r(var, value[name], pnum)
					else
						debug_print_var(var, value[name], 0, pnum, var)
					end
					find = true
					break						
				else
					value = value[name]
				end
			else
				localpushstring(sformat("%s = %s    [type:%s]", var, tostring(value[name]), type(value[name])))
				find = true
				break
			end
		end
	
		dep_num = dep_num + 1
	end
	
	return find
end

--打印变量的值
local function debug_print_expr(var, pnum, depnum)
	if not var then
		localpushstring(tostring(var).." is invalid.")
		return
	end
	
	--清空临时变量表	
	deep_table = {}

	if var == "G" or (var == "_G" and pnum == 1) then
		for i, v in pairs(_G) do
			localpushstring(tostring(i).."    "..tostring(v))
		end
		return 
	end
	
	local find  = false	
	
	if sfind(var,"[_%a][%.%[][_%w]*") then --多个.或[]名称修饰
		--在全局以及局部环境中找
		local first_name = ssub(var, sfind(var,"[_%a][_%w]*"))
		if first_name == "_G" then
			find = loop_print_var(var,_G, pnum)
		end
		if not find then
			local index = 1
			local name, value
			while true do
				name, value = dgetlocal(depnum, index)
				if not name then break end
				index = index + 1

				if name == first_name then
					find = loop_print_var(var,value, pnum)
				end
			end
		end		
	else
		--找局部变量
		local index = 1
		local name, value
		while true do
			name, value = dgetlocal(depnum, index)
			if not name then 
				break 
			end
			index = index + 1

			if name == var then
				if s_usedtree then
					print_r(var, value, pnum)
				else
					debug_print_var(var, value, 0, pnum, var)
				end
				
				find = true
				return
			end
		end

		--找全局变量
		if _G[var] ~= nil then
			if s_usedtree then
				print_r(var, _G[var], pnum)
			else
				debug_print_var(var, _G[var], 0, pnum, var)
			end
			find = true
			return
		end	

	end
	
	if not find then
		localpushstring('No symbol "'..tostring(var)..'" in current context.')
	end
end

--发送某个变量的值
local function sendvariablevalue(var, pnum)
	s_num = 0
	s_msg = packet.anyfor_ldb()
	packet.settype(s_msg, 0)
	packet.pushint16(s_msg, s_num)
	debug_print_expr(var, pnum, 6)
	packet.pushint16toindex(s_msg, 0, s_num)
	sendmsg(s_msg)
	s_msg = nil
	s_num = 0
end

--执行一次命令
local function execute_once(cmd)
	local env = dgetinfo(4)
	local c = cmd
	local arglist
	local rs = sfind(cmd, " ")
	if rs ~= nil then
		c = ssub(cmd, 1, rs - 1)
		arglist = ssub(cmd, rs + 1)
	end
	if deep_table then
		deep_table = nil
	end
	if c == "c" then
		s_debugmgr.trace = false
		--尝试转入高速模式。函数niltrace会判别当前运行模式
		if  dgethook() then
			if (s_debugmgr.breaktable.num == 0) or (s_debugmgr.breaktable.validnum == 0) then
				niltrace()
			end
		end
		return true
	elseif c == "s" then
		s_debugmgr.trace = true
		if not dgethook() then
			checksethook()
		end
		return true
	elseif c == "n" then
		s_debugmgr.trace = false
		s_debugmgr.nextstep = true
		s_debugmgr.current_func = env.func
		s_debugmgr.trace_count = get_functionframe_deep() - 1
		if not dgethook() then
			checksethook()
		end
		return true
	elseif c == "p" then
		sendvariablevalue(arglist, -1)
	elseif c == "pt" then
		sendvariablevalue(arglist, 1)
	elseif c == "see" then
		sendvariablevalue(arglist, -1)
		return true
	elseif c == "seet" then
		sendvariablevalue(arglist, 1)
		return true
	elseif c == "printtree" then
		local msg = packet.anyfor_ldb()
		packet.settype(msg, 0)
		packet.pushint16(msg, 1)
		packet.pushstring(msg, "Use tree structure output.\n")
		sendmsg(msg)
		s_usedtree = true
		return true
	elseif c == "printtable" then
		local msg = packet.anyfor_ldb()
		packet.settype(msg, 0)
		packet.pushint16(msg, 1)
		packet.pushstring(msg, "Use table structure output.\n")
		sendmsg(msg)
		s_usedtree = false
		return true
	elseif c == "b" then
		local rs = sfind(arglist, ":")
		local filename = ssub(arglist, 1, rs - 1)
		local line = ssub(arglist, rs + 1)
		line = tonumber(line)
		local msg = packet.anyfor_ldb()
		packet.settype(msg, 0)
		packet.pushint16(msg, 1)
		packet.pushstring(msg, addbreakpoint(line, filename) ..'\n')
		sendmsg(msg)
	elseif c == "d" then
		local msg = packet.anyfor_ldb()
		packet.settype(msg, 0)
		packet.pushint16(msg, 1)
		packet.pushstring(msg, delbreakpoint(arglist) ..'\n')
		sendmsg(msg)
	elseif c == "bl" then
		sendbreakpointlist()
	elseif c == "be" then
		local msg = packet.anyfor_ldb()
		packet.settype(msg, 0)
		packet.pushint16(msg, 1)
		packet.pushstring(msg, enablebreakpoint(arglist) .. '\n')
		sendmsg(msg)
	elseif c == "bd" then
		local msg = packet.anyfor_ldb()
		packet.settype(msg, 0)
		packet.pushint16(msg, 1)
		packet.pushstring(msg, disablebreakpoint(arglist) .. '\n')
		sendmsg(msg)
	elseif c == "bt" then
		send_tracebackinfo()
	elseif c == "l" then
		--回馈当前的行列表
		sendlist_source()
	elseif c == "q" then
		stopdebug()
		return true
	end

	return false
end

--阻塞式等待客户端命令
local function waitcommand ()
	socketer.realrecv(s_netmgr.client)
	local msg
	while true do
		if socketer.isclose(s_netmgr.client) then
			return "q"
		end
		msg = socketer.getmsg_ldb(s_netmgr.client)
		if msg then
			break
		else
			delay(10)
		end
	end
	packet.begin(msg)
	local str = packet.getstring(msg)
	return str
end

--执行命令
local function execute_command (cmd)
	local res
	while true do
		res = execute_once(cmd)
		s_debugmgr.lastcmd = cmd
		if res then
			break
		end
		cmd = waitcommand()
		socketer.realsend(s_netmgr.client)
	end
end

--初始化网络
local function initnetwork()
	--直接初始化，不用判断是否失败
	local res = lxnet.init(4096, 100, 2, 10, 1)
	s_netmgr.listen = listener.create()
	assert(s_netmgr.listen, "create listener object failed!")
	local res = listener.listen(s_netmgr.listen, s_netmgr.port, 1)
	if not res then
		log_error(sformat("[luafile: %s] [line: %d]: listen %d  failed!", dgetinfo(1, "S").source, dgetinfoo(1, "l").currentline, s_netmgr.port))
		os.exit(1)
	end
	if res then
		s_netmgr.needrelease = true
	end
end

--阻塞式等待客户端接入
local function waitclient ()
	if s_netmgr.client then
		return
	end
	while not s_netmgr.client do
		if listener.canaccept(s_netmgr.listen) then
			s_netmgr.client = listener.accept(s_netmgr.listen)
		else
			delay(100)
		end
	end
	socketer.realsend(s_netmgr.client)
	socketer.realrecv(s_netmgr.client)
end

--非阻塞方式获取命令
local function noblockwaitcommand()
	socketer.realsend(s_netmgr.client)
	socketer.realrecv(s_netmgr.client)
	local str
	local msg = socketer.getmsg_ldb(s_netmgr.client)
	if msg then
		packet.begin(msg)
		str = packet.getstring(msg)
	end
	return str
end

--行事件的回调函数
local function trace(event, line)
	local env = dgetinfo(2)

	--不能调试此文件
	if s_thisfilename == env.source then
		return
	end	

	--为单步调试 n
	if s_debugmgr.nextstep then
		local trace_count = get_functionframe_deep()
		--函数返回后，则调用栈会比当时记录的小
		if trace_count < s_debugmgr.trace_count then
			s_debugmgr.nextstep = false
			s_debugmgr.trace = true
		elseif trace_count == s_debugmgr.trace_count then
			if s_debugmgr.current_func == env.func then
				s_debugmgr.nextstep = false
				s_debugmgr.trace = true
			end
		end
	end

	local touchbpstr = nil

	--断点处理
	if not s_debugmgr.trace and s_debugmgr.breaktable.tb[line] then
		local source = ssub(env.source, 2, #env.source)
		source = getfilefullname(source)
		local bp = s_debugmgr.breaktable.tb[line][source]
		if bp and bp.active then
			s_debugmgr.nextstep = false
			s_debugmgr.trace = true

			--告知遇到断点了
			touchbpstr = "Breakpoint "..tostring(bp.number)..", "..(env.name or "unknow").." at "..bp.source..":"..tostring(bp.line)
		end
	end

	--为单步调试时 s
	if s_debugmgr.trace then
		--回馈这行的信息
		local filename = ssub(env.source, 2, #env.source)
		local retlines, num = getfileline(filename, line, 1)
		local msg = packet.anyfor_ldb()
		packet.settype(msg, 0)
		if touchbpstr then
			num = num + 1
		end
		packet.pushint16(msg, num)
		if touchbpstr then
			packet.pushstring(msg, touchbpstr .. '\n')
		end
		for k, v in pairs(retlines) do
			packet.pushstring(msg, v .. '\n')
		end
		sendmsg(msg)

		--阻塞获取一个命令，并执行
		execute_command(waitcommand())
	else
		--如果是纯脚本，则非阻塞检测下是否有新命令
		if s_debugmgr.runmodule == "purelua" then
			--非阻塞方式接收命令，然后执行。
			local src = noblockwaitcommand()
			if src then
				execute_command(src)
			end
		else
			if not s_debugmgr.nextstep then
				if s_debugmgr.runmodule == "useframefunc" then
				--为useframefunc模式，则无断点的话就去掉行hook
					if  dgethook() then
						if (s_debugmgr.breaktable.num == 0) or (s_debugmgr.breaktable.validnum == 0) then
							niltrace()
						end
					end
				end
			end
		end
	end
end

--当收到s或者n命令时，看看hook函数是否存在，若不存在则设置
function checksethook()
	if not dgethook() then
		dsethook(trace, "l")
	end
end

--当断点数量或者被激活的断点数量变更为0，则根据相关状态来决定运行模式；分为高速模式与低速模式
function niltrace()
	--当启用调试纯脚本的接口时，没有主循环，所以。。。只能采用低速模式。
	if s_debugmgr.runmodule == "purelua" then
		return
	end
	if s_debugmgr.runmodule == "useframefunc" then
		dsethook()
	end
end

--当断点数量或者被激活的断点数量从0变更为大于0时。若没设置行事件，则设置
function userlinetrace()
	if not dgethook() then
		dsethook(trace, "l")
	end
end



-------------------------------------------------------------------------------
-------------------------------对外使用接口------------------------------------
-------------------------------------------------------------------------------

--以使用每帧调用函数的方式启动调试
function startdebug_use_loopfunc(port)
	if not port then
		port = 0xdeb
	end
	--防止重复
	if s_netmgr.port then
		return
	end

	s_netmgr.port = port
	--初始化网络以及调试相关数据
	initnetwork()

	--记录下当前使用模式为使用帧函数
	s_debugmgr.runmodule = "useframefunc"

	dsethook(trace, "l")
end

--每帧调用下此函数（此函数内所有操作都是非阻塞的）
function debug_runonce()
	if s_netmgr.needrelease then
		lxnet.run()
	end
	--先检查下原有的连接；
	if s_netmgr.client then
		if socketer.isclose(s_netmgr.client) then

			--原有连接断开，则当做退出命令处理
			execute_command("q")
			return
		end
	end

	if s_netmgr.client then
		--非阻塞方式接收命令，然后执行。
		local src = noblockwaitcommand()
		if src then
			execute_command(src)
		end
	else
		--尝试接受新的宿主连接
		if listener.canaccept(s_netmgr.listen) then
			s_netmgr.client = listener.accept(s_netmgr.listen)
			if s_netmgr.client then
				socketer.realsend(s_netmgr.client)
				socketer.realrecv(s_netmgr.client)
			end
		end
	end	
end

--当不允许宿主调试时时，调用此函数
function stopdebug()
	--释放网络以及调试相关数据
	s_debugmgr.breaktable.num = 0
	s_debugmgr.breaktable.validnum = 0
	s_debugmgr.breaktable.tb = {}

	if s_netmgr.client then
		socketer.release(s_netmgr.client)
		s_netmgr.client = nil
	end
	dsethook()
	deep_table = nil
end

--调试纯Lua脚本时调用
function debug_purelua(port)
	startdebug_use_loopfunc(port)
	
	--记录下当前使用模式为纯脚本，切记放到函数startdebug_use_loopfunc 后
	s_debugmgr.runmodule = "purelua"
	waitclient()
	execute_command(waitcommand())
end


