--[[

Copyright (C) lcinx
lcinx@163.com

]]

require "lxnet"

--管理网络部分
local s_netmgr = {ip = "", port = 0, con = nil}

--管理调试部分
local s_debugmgr = {
	currentstate = "unknow",	--状态： 等待调试者输入命令: waitinput   等待被调试者回馈: waitrecv
	lastcmd = "",				--上一次调试命令
	filelog = nil,				--日志对象，若非空，则写日志；否则不写
}

local function inputcommand()
	io.write("(ldb) ")
	return io.read()
end

local function print_help()
	print("h                            help info")
	print("c                            continue")
	print("s                            step into")
	print("n                            next")
	print("p var                        print variable")
	print("pt tablename                 print table key, value")
	print("see var                      print variable, but not block!")
	print("seet tablename               print table key, value, but not block!")
	print("b ./filename/src.lua:line    add breakpoint")
	print("d num/d *                    del breakpoint/del all breakpoint")
	print("bl                           list breakpoint")
	print("be num/be *                  enable breakpoint/enable all breakpoint")
	print("bd num/bd *                  disable breakpoint/disable all breakpoint")
	print("bt                           print traceback")
	print("l                            list source code")
	print("q                            quit debug")
	print("printtree                    print info tree format")
	print("printtable                   print info table format")
	print("openlog                      write debug all info file log")
	print("closelog                     not write debug all info file log")
end

--输出并写日志
local function printandtrywritelog(str)
	if s_debugmgr.filelog then
		s_debugmgr.filelog:write(str)
	end
end


--发送命令
local function sendcommand(cmd)
	local msg = packet.anyfirst()
	packet.pushstring(msg, cmd)
	socketer.sendmsg(s_netmgr.con, msg)
	socketer.realsend(s_netmgr.con)
	socketer.realrecv(s_netmgr.con)
end

local s_watirecv_num = 0

--等待回馈，若连接断开，返回nil；否则，返回消息
local function waitrecvmsg()
	s_watirecv_num = s_watirecv_num + 1
	local msg 
	while true do
		--检查连接
		if socketer.isclose(s_netmgr.con) then
			s_watirecv_num = s_watirecv_num - 1
			return nil
		end

		--尝试获取消息
		msg = socketer.getmsg(s_netmgr.con)
		if msg then
			s_watirecv_num = s_watirecv_num - 1
			return msg
		end
		delay(10)
	end
end

local function check_command(c, arglist)
	if c == "c" then
		return true
	elseif c == "s" then
		return true
	elseif c == "n" then
		return true
	elseif c == "p" then
		if not arglist then
			return false
		end
		return true
	elseif c == "pt" then
		if not arglist then
			return false
		end
		return true
	elseif c == "see" then
		if not arglist then
			return false
		end
		return true
	elseif c == "seet" then
		if not arglist then
			return false
		end
		return true
	elseif c == "b" then
		if not arglist then
			return false
		else
			--看看是否为filename:line
			local rs = string.find(arglist, ":")
			if not rs then
				return false
			end
			local filename = string.sub(arglist, 1, rs - 1)
			--检查文件名是否为.lua后缀
			if not string.find(filename, ".lua") then
				return false
			end
			local line = string.sub(arglist, rs + 1)
			if filename == "" or line == "" then
				return false
			end
			local _, num = string.gsub(line, "[%d]", "")
			if num ~= #line then
				return false
			end
		end
		return true
	elseif c == "d" then
		if not arglist then
			return false
		else
			--看看是否为数字或者*
			if arglist == "*" then
				return true
			else
				local _, num = string.gsub(arglist, "[%d]", "")
				--参数有问题，不都是数字？
				if num ~= #arglist then
					return false
				else
					return true
				end
			end
		end
	elseif c == "bl" then
		return true
	elseif c == "be" then
		if not arglist then
			return false
		else
			--看看是否为数字或者*
			if arglist == "*" then
				return true
			else
				local _, num = string.gsub(arglist, "[%d]", "")
				--参数有问题，不都是数字？
				if num ~= #arglist then
					return false
				else
					return true
				end
			end
		end
	elseif c == "bd" then
		if not arglist then
			return false
		else
			--看看是否为数字或者*
			if arglist == "*" then
				return true
			else
				local _, num = string.gsub(arglist, "[%d]", "")
				--参数有问题，不都是数字？
				if num ~= #arglist then
					return false
				else
					return true
				end
			end
		end
	elseif c == "bt" then
		return true
	elseif c == "l" then
		return true
	elseif c == "printtree" then
		return true
	elseif c == "printtable" then
		return true
	end
	return nil
end

--解析回馈，并做一些处理
local function parserecvmsg(msg)
	packet.begin(msg)
	local resnum = packet.getint16(msg)
	local resstr
	while resnum > 0 do
		resstr = packet.getstring(msg)
		if resstr == "" then
			break
		end
		resstr = string.gsub(resstr, "\t", "    ")
		if string.find(resstr, "\n") == 1 then
			resstr = string.sub(resstr, 2, #resstr)
		end
		io.write(resstr)
		printandtrywritelog(resstr)
		resnum = resnum - 1
	end
end

--执行用户命令
local function execute_command(cmd)
	if cmd == "" then
		cmd = s_debugmgr.lastcmd
	end
	

	local sendcmd
	local c = cmd
	local arglist
	local rs = string.find(cmd, " ")
	if rs ~= nil then
		c = string.sub(cmd, 1, rs - 1)
		arglist = string.sub(cmd, rs + 1)
	end
	c = string.lower(c)
	sendcmd = c
	if arglist then
		sendcmd = sendcmd .. " "..arglist
	end

	if c == "help" or c == "h" then
		print_help()
	elseif c == "openlog" then
		if not s_debugmgr.filelog then
			s_debugmgr.filelog = io.open("ldb_log.log", "a")
		end
		print("Now is writing log.")
	elseif c == "closelog" then
		if s_debugmgr.filelog then
			s_debugmgr.filelog:close()
			s_debugmgr.filelog = nil
		end
		print("Now I didn't write log.")
	elseif c == "quit" or c == "q" then
		return false
	else
		--检查命令，若格式合法，则告知目标机执行。
		local rest = check_command(c, arglist)
		if rest == true then
			--发送命令
			sendcommand(sendcmd)
			printandtrywritelog(sendcmd..'\n')

			--等待回馈
			s_debugmgr.currentstate = "waitrecv"
			local msg
			while true do
				msg = waitrecvmsg()
				s_debugmgr.currentstate = "unknow"
				if not msg then
					print("wait recv msg failed!, disconnect")
					return false
				end
				--解析回馈并做一些处理
				parserecvmsg(msg)
				if packet.gettype(msg) == 0 then
					break
				end
			end
		elseif rest == false then
			print('Command arguments Invalid. command: '..cmd)
		elseif rest == nil then
			--命令错误
			print('Undefined command: "'.. cmd ..'". Try "Help".')
		end
	end
	s_debugmgr.lastcmd = cmd
	return true
end

--当控制台触发了ctrl+c事件，则处理下
function onctrl_hander()
	--若当前是等待输入命令，则显示一条提示信息
	if s_debugmgr.currentstate == "waitinput" then
		print("Quit <expect signal SIGNT when the program is resumed>")
	elseif s_debugmgr.currentstate == "waitrecv" then
	--若当前是等待被调试端回馈，则就是告知下被调试端进入单步
		if s_watirecv_num ~= 0 then
			sendcommand("s")
		else
			execute_command("s")
		end
	end
end


local function start()
	--初始化网络模块
	if not lxnet.init(4096, 100, 2, 10, 1) then
		print("error: init network failed!")
		return
	end
	
	s_netmgr.con = socketer.create()
	if not s_netmgr.con then
		print("error: create socketer failed!")
		return
	end
	--连接目标端
	while true do
		if socketer.connect(s_netmgr.con, s_netmgr.ip, s_netmgr.port) then
			break
		end
		delay(20)
	end
	socketer.realrecv(s_netmgr.con)
	
	print("Warning, not greater than 30kb lua source line.\nlxnet ldb 1.0\nCopyright (C) lcinx\nlcinx@163.com\nTry help command\n\n")

	s_debugmgr.currentstate = "waitinput"
	local command = inputcommand()
	s_debugmgr.currentstate = "unknow"
	--进入调试主循环，从控制台获取命令，发送给目标端执行，接收目标机回馈并显示。
	while true do
		if socketer.isclose(s_netmgr.con) then
			socketer.release(s_netmgr.con)
			s_netmgr.con = nil
			print("disconnect!")
			break
		end
		if not execute_command(command) then
			print("execute command failed!")
			break
		end
		s_debugmgr.currentstate = "waitinput"
		command = inputcommand()
		s_debugmgr.currentstate = "unknow"
		delay(10)
	end
end

local function inputipport()
	print("Input the target machine IP:")
	s_netmgr.ip = io.read()
	print("Input the target machine port:")
	s_netmgr.port = io.read()
end

s_netmgr.ip = "127.0.0.1"
s_netmgr.port = 0xdeb
inputipport()
start()
if s_debugmgr.filelog then
	s_debugmgr.filelog:close()
end
os.execute("pause")
