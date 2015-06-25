它是什么：<br />

> 是一个lua 的远程调试器。<br />

使用平台：<br />

> Windows（32位可用，64位暂时未测试） 与 Linux（32位和64位都可用）

<br>
<br><a href='http://code.google.com/p/ldb/downloads/list'><font color='#ff0000' size='10'><b>最新使用手册</b></font></a><br>

<br />
简略的说明：<br>
<blockquote>文件lxdebug.lua为被调试者<br />
文件ldbclient.lua为调试者</blockquote>

适用场合、范围:<br />

<ol><li>被调试端是纯脚本。  此时切记在脚本最开始执行的地方如此写:<br>
<blockquote>local ldb = require"lxdebug"<br />
ldb.debug_purelua()  --此函数在lxdebug.lua里定义，切记先明白何时调用何种接口。 参数为端口。若不指定，则端口为0xdeb。端口被占用则会无法启动，并有错误日志。<br />
</blockquote></li></ol><blockquote>2. 被调用者存在逻辑帧的概念。（可以理解为间隔比较小的时间，比如数百ms之内， 会重复进入某个函数）。 此时最好别用纯脚本的调试接口，因为他不支持高速模式。所谓高速模式：当存在逻辑帧概念的时候，会根据断点有效情况等，来决定是否hook行事件，若无断点，则其额外开销极小，就是在逻辑帧函数中做一下检测，而不是执行每行时都检测下。使用方法如下:<br>
<blockquote>初始化的文件中：<br>
<blockquote>local ldb = require"lxdebug"<br />
ldb.startdebug_use_loopfunc()--可以传入端口；默认为0xdeb<br />
在逻辑帧执行的函数中执行如下函数:<br>
ldb.debug_runonce()<br />
当要停止当前的调试行为或者要清除，则调用：<br>
ldb.stopdebug()<br /></blockquote></blockquote></blockquote>


目前存在的问题：<br />
如下：<br />
a.lua 内容：<br />
1. local s_mgr = {}<br />
2. function aa()<br />
3.     local aa1 = 3<br />
4.     aa1 = aa1 + 1<br />
5. end<br />

b.lua内容:<br />
require "a"<br />
aa()<br />

入口为函数b。<br>
<blockquote>当在函数aa中下断点，执行到第四行时，查看aa1时，值为3，而到第五行，则查看会提示找不到符号aa1，不过这个暂时可以容忍，记得vc里，看最后一行时，若其有改变局部变量的行为，也会看不到（也可能记错。），此问题暂且不论，有另一个比较不爽的问题。<br />    当在第四行时，看a.lua里的文件局部变量s_mgr时，无法看。。。暂时没找到如何获取，若有哪位朋友知道如何获取，可以告知下我吗？ email:lcinx@163.com