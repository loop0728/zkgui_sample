#pragma once
/*
*此文件由GUI工具生成
*文件功能：用于处理用户的逻辑相应代码
*功能说明：
*========================onButtonClick_XXXX
当页面中的按键按下后系统会调用对应的函数，XXX代表GUI工具里面的[ID值]名称，
如Button1,当返回值为false的时候系统将不再处理这个按键，返回true的时候系统将会继续处理此按键。比如SYS_BACK.
*========================onSlideWindowItemClick_XXXX(int index) 
当页面中存在滑动窗口并且用户点击了滑动窗口的图标后系统会调用此函数,XXX代表GUI工具里面的[ID值]名称，
如slideWindow1;index 代表按下图标的偏移值
*========================onSeekBarChange_XXXX(int progress) 
当页面中存在滑动条并且用户改变了进度后系统会调用此函数,XXX代表GUI工具里面的[ID值]名称，
如SeekBar1;progress 代表当前的进度值
*========================ogetListItemCount_XXXX() 
当页面中存在滑动列表的时候，更新的时候系统会调用此接口获取列表的总数目,XXX代表GUI工具里面的[ID值]名称，
如List1;返回值为当前列表的总条数
*========================oobtainListItemData_XXXX(ZKListView::ZKListItem *pListItem, int index)
 当页面中存在滑动列表的时候，更新的时候系统会调用此接口获取列表当前条目下的内容信息,XXX代表GUI工具里面的[ID值]名称，
如List1;pListItem 是贴图中的单条目对象，index是列表总目的偏移量。具体见函数说明
*========================常用接口===============
*LOGD(...)  打印调试信息的接口
*mTextXXX->setText("****") 在控件TextXXX上显示文字****
*mButton1->setSelected(true); 将控件mButton1设置为选中模式，图片会切换成选中图片，按钮文字会切换为选中后的颜色
*mSeekBar->setProgress(12) 在控件mSeekBar上将进度调整到12
*mListView1->refreshListView() 让mListView1 重新刷新，当列表数据变化后调用
*mDashbroadView1->setTargetAngle(120) 在控件mDashbroadView1上指针显示角度调整到120度
*
* 在Eclipse编辑器中  使用 “alt + /”  快捷键可以打开智能提示
*/

#include "test/TouchEventTest.h"
#include "uart/ProtocolSender.h"

static void SendClickEvent(int v = 0) {
  BYTE data[2] = {0, v};
  sendProtocol(CMD_BUTTON_ON, data, sizeof(data));
}

/**
 * 注册定时器
 * 在此数组中添加即可
 */
static S_ACTIVITY_TIMEER REGISTER_ACTIVITY_TIMER_TAB[] = {
	//{0,  6000}, //定时器id=0, 时间间隔6秒
	//{1,  1000},
};

static void onUI_init(){
    //Tips :添加 UI初始化的显示代码到这里,如:mText1->setText("123");
	EASYUICONTEXT->hideStatusBar();
#ifdef DISABLE_HELP_INFO
	mButton4Ptr->setVisible(false);
#endif
}

static void onUI_quit() {
//	EASYUICONTEXT->showStatusBar();
}


static void onProtocolDataUpdate(const SProtocolData &data) {
    // 串口数据回调接口
}

static bool onUI_Timer(int id){
    //Tips:添加定时器响应的代码到这里,但是需要在本文件的 REGISTER_ACTIVITY_TIMER_TAB 数组中 注册
    //id 是定时器设置时候的标签,这里不要写耗时的操作，否则影响UI刷新,ruturn:[true] 继续运行定时器;[false] 停止运行当前定时器
    return true;
}


static bool ontestButtonActivityTouchEvent(const MotionEvent &ev) {
    // 返回false触摸事件将继续传递到控件上，返回true表示该触摸事件在此被拦截了，不再传递到控件上
    return false;
}
static bool onButtonClick_Buttonbg(ZKButton *pButton) {
    pButton->setSelected(!pButton->isSelected());
	SendClickEvent();
    return true;
}

static bool onButtonClick_Buttonsw(ZKButton *pButton) {
    //LOGD(" ButtonClick Buttonsw !!!\n");
	pButton->setSelected(!pButton->isSelected());
	SendClickEvent(pButton->isSelected());
    return true;
}

static bool onButtonClick_Buttoncheck(ZKButton *pButton) {
    //LOGD(" ButtonClick Buttoncheck !!!\n");
	pButton->setSelected(!pButton->isSelected());
	SendClickEvent(pButton->isSelected());
    return true;
}

static bool onButtonClick_sys_back(ZKButton *pButton) {
    //LOGD(" ButtonClick sys_back !!!\n");
    return false;
}

/*
static bool onButtonClick_Button1(ZKButton *pButton) {
    //LOGD(" ButtonClick Button1 !!!\n");
	pButton->setVisible(false);
	mButton2Ptr->setVisible(true);
    return true;
}
*/

static bool onButtonClick_Buttonspecial(ZKButton *pButton) {
    //LOGD(" ButtonClick Buttonspecial !!!\n");
	static char value = '0';
	value+=1;
	if(value > '9'){
		value = '0';
	}
	pButton->setText(value);
	SendClickEvent(value - '0');
    return true;
}

static bool onButtonClick_Button1(ZKButton *pButton) {
	LOGD("onButtonClick_Button1\n");
	static int lastClickTime = 0;
	static int clickCount = 0;

	int curTime = clock() / 1000;
	if (curTime - lastClickTime <= 1000) {
		clickCount++;
		if (clickCount == 10) {
			TouchEventTest::getInstance()->startTest();
			EASYUICONTEXT->goHome();
		}
	} else {
		LOGD("onButtonClick_Button1 clickCount 1\n");
		clickCount = 1;
	}

	lastClickTime = curTime;

    return true;
}

static bool onButtonClick_Button2(ZKButton *pButton) {
    //LOGD(" ButtonClick Button2 !!!\n");
	pButton->setVisible(false);
    return true;
}

static bool onButtonClick_Button_open_developer(ZKButton *pButton) {
    SendClickEvent();
    return true;
}
static bool onButtonClick_Button3(ZKButton *pButton) {
    //LOGD(" ButtonClick Button3 !!!\n");
    return true;
}

static bool onButtonClick_Button4(ZKButton *pButton) {
    EASYUICONTEXT->openActivity("helpButtonActivity");
    return false;
}
