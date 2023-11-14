#!/bin/bash
# ToDo:此shell脚本为了显示弹窗告警！！
b=1;
while true :
do
# 获取文件内容的行数
a=$(awk '{print NR}' ./桌面/xxx | tail -n 1);

#echo $a;
if ((a > b));then
	# 获取文件最后一行的内容
	pid=$(sed -n '$p' ./桌面/xxx);
	echo $pid;
	pidinfo=$(ps axhw -o user,pid,cmd | grep -v grep|grep $pid);
	echo "$(date "+%Y-%m-%d-%H:%M:%S") $pidinfo";
	/usr/bin/notify-send -i face-smile 'Wa:__NR_perf_event_open Running hading '$a' times!!!' "The one time is : $(date "+%Y-%m-%d-%H:%M:%S !!!!")\\nit is: $pidinfo";
	#zenity 调用的弹窗会线程中止，可能会漏掉通知！！，演示时弹出后及时点掉再继续运行程序问题不大!
	zenity --warning --title=Warning:Pert_Event_Open_is_Running --text="__NR_perf_event_open_API is Running Now $(date)\\n Info:   $pidinfo" --width=1 --timeout=60 --modal;
	b=$a;
	echo "info:$b";
fi
#echo "over!"
done





