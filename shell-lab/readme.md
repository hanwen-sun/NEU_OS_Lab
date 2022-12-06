## minishell
本实验基于csapp的lab6 shell-lab, 拓展实现了 | pipe 和 > 重定向功能。

minishell支持的功能:

siganl caputre: foreground运行的进程可以捕捉control c(终止), control z(暂停)等信号;

built-in command:

    quit: 退出进程;
    jobs: 显示当前在后台运行的进程信息;
    fg/bg: bg命令指定当前暂定的进程/job后台运行。 fg命令指定当前运行的后台进程到前台运行。

作业管理:
    
    minishell支持进程管理， job数据结构表示一个shell中执行的作业， 一个作业可能由一个或多个进程组成, 作业开始时addjob, 结束时deletejob, 状态改变时也要改变job;

| 和 > 拓展实现:

    > 重定向较容易实现, 相应的可以实现 >>，< 和 <<
    | 的实现尚有问题: 1. 未能实现多个 | 的相连 2. 运行 | 输出结果后无法正常退出进程。 主要实现方法是通过pipe重定向传输数据, 将left_command的pipe输出与标准输出重定向, right_command的pipe输入与标准输入重定向。

测试:

    见makefile文件, trace01 - trace15都为测试文件, myint, myspin, mysplit辅助测试。将运行结果与tshref.out中相对比。 tshref.out中的结果是在真实shell中的运行结果。
