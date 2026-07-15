#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ========== WiFi 配置（STA 模式） ==========
const char* ssid = "WiFi";
const char* password = "12345678";

// ESP-01S 可用 GPIO：0, 1(TX), 2, 3(RX)
// GPIO0 控制灯（LED 负极接 GPIO0，正极串 220Ω 电阻接 3.3V）
const int LED_PIN = 0;

ESP8266WebServer server(80);
bool ledState = false;

// 定时任务结构
struct TimerTask {
    uint8_t hour;
    uint8_t minute;
    uint8_t days;    // 位掩码: bit0=周日, bit1=周一...bit6=周六, bit7=每天
    bool action;     // true=开灯, false=关灯
    bool enabled;
    bool executed;   // 仅一次任务用，标记已执行
};

TimerTask timers[10];
int timerCount = 0;

// ========== HTML 页面 ==========
const char* htmlPage = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>智能灯控</title>
    <style>
        * { margin:0; padding:0; box-sizing:border-box; -webkit-tap-highlight-color: transparent; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: linear-gradient(160deg, #fef3e2 0%, #fde8cd 40%, #f5d0a9 100%);
            min-height: 100vh; padding-bottom: 30px;
            position: relative;
        }
        body::before {
            content: ''; position: fixed; top: -60px; right: -80px; z-index: -1;
            width: 260px; height: 260px; border-radius: 50%;
            background: rgba(249, 115, 22, 0.12); filter: blur(60px); pointer-events: none;
        }
        body::after {
            content: ''; position: fixed; bottom: -40px; left: -60px; z-index: -1;
            width: 220px; height: 220px; border-radius: 50%;
            background: rgba(251, 191, 36, 0.15); filter: blur(50px); pointer-events: none;
        }
        #app { max-width: 420px; margin: 0 auto; padding: 0 16px; }

        .header {
            display: flex; align-items: center; justify-content: space-between;
            padding: 20px 0 12px;
        }
        .header-left { display: flex; flex-direction: column; }
        .header-left h1 {
            font-size: 26px; font-weight: 700;
            background: linear-gradient(135deg, #f97316 0%, #ea580c 100%);
            -webkit-background-clip: text; -webkit-text-fill-color: transparent;
            background-clip: text; letter-spacing: -0.5px;
        }
        .header-left .sub {
            font-size: 13px; color: #9ca3af; margin-top: 2px;
            font-weight: 500; letter-spacing: 0.5px;
        }
        .frost-card {
            background: rgba(255, 255, 255, 0.55);
            backdrop-filter: blur(20px) saturate(1.4);
            -webkit-backdrop-filter: blur(20px) saturate(1.4);
            border: 1px solid rgba(255, 255, 255, 0.65);
            border-radius: 20px;
            box-shadow: 0 4px 24px rgba(0,0,0,0.04), inset 0 1px 0 rgba(255,255,255,0.6);
            transition: border-color 0.2s, box-shadow 0.2s;
        }
        .frost-card.active {
            border-color: rgba(249, 115, 22, 0.4);
            box-shadow: 0 4px 24px rgba(249,115,22,0.08), inset 0 1px 0 rgba(255,255,255,0.6);
        }

        .power-card { padding: 36px 20px 28px; text-align: center; margin-bottom: 16px; }
        .power-btn {
            width: 100px; height: 100px; border-radius: 50%;
            border: 3px solid rgba(229,231,235,0.6);
            background: rgba(255,255,255,0.7);
            backdrop-filter: blur(8px); -webkit-backdrop-filter: blur(8px);
            display: flex; align-items: center; justify-content: center;
            margin: 0 auto 18px; cursor: pointer;
            transition: all 0.35s cubic-bezier(0.34,1.56,0.64,1);
        }
        .power-btn svg { width: 44px; height: 44px; color: #9ca3af; transition: color 0.2s; }
        .power-btn.on {
            border-color: #f97316; background: #f97316;
            box-shadow: 0 8px 32px rgba(249,115,22,0.35);
            transform: scale(1.05);
        }
        .power-btn.on svg { color: white; }
        .power-text { font-size: 20px; font-weight: 600; transition: 0.3s; }
        .power-sub { font-size: 13px; color: #9ca3af; margin-top: 6px; font-weight: 500; }

        .section { padding: 18px; margin-bottom: 14px; }
        .section-header {
            display: flex; align-items: center; gap: 10px;
            font-size: 15px; font-weight: 600; color: #374151;
            margin-bottom: 14px;
        }
        .section-header svg { width: 18px; height: 18px; color: #f97316; }
        .count { margin-left: auto; font-size: 13px; color: #9ca3af; font-weight: 500; }

        .timer-list { display: flex; flex-direction: column; gap: 10px; }
        .timer-item {
            display: flex; align-items: center;
            padding: 14px 14px;
            background: rgba(255,255,255,0.45);
            backdrop-filter: blur(8px); -webkit-backdrop-filter: blur(8px);
            border: 1px solid rgba(255,255,255,0.55);
            border-radius: 16px;
            transition: all 0.2s ease;
        }
        .timer-item:hover { background: rgba(255,255,255,0.7); transform: translateY(-1px); }
        .timer-time { font-size: 22px; font-weight: 600; color: #1f2937; width: 70px; font-variant-numeric: tabular-nums; }
        .timer-info { flex: 1; margin-left: 12px; }
        .timer-days { font-size: 12px; color: #6b7280; font-weight: 500; }
        .timer-action { font-size: 13px; font-weight: 600; margin-top: 3px; }
        .timer-action.on { color: #f97316; }
        .timer-action.off { color: #ef4444; }
        .timer-switch {
            width: 46px; height: 26px; border-radius: 13px;
            background: rgba(209,213,219,0.6); position: relative; cursor: pointer;
            transition: 0.3s; flex-shrink: 0; margin-right: 10px;
        }
        .timer-switch::after {
            content: ''; position: absolute; width: 22px; height: 22px;
            background: white; border-radius: 50%; top: 2px; left: 2px;
            transition: 0.3s; box-shadow: 0 1px 4px rgba(0,0,0,0.15);
        }
        .timer-switch.active { background: #f97316; }
        .timer-switch.active::after { left: 22px; }
        .timer-delete {
            width: 32px; height: 32px; border: none; background: transparent;
            border-radius: 8px; display: flex; align-items: center; justify-content: center;
            color: #9ca3af; transition: all 0.2s; opacity: 1;
        }
        .timer-delete:hover { background: rgba(239,68,68,0.1); color: #ef4444; }
        .timer-delete svg { width: 18px; height: 18px; }

        .add-btn {
            width: 100%; padding: 14px; margin-top: 12px;
            border: 2px dashed rgba(249,115,22,0.3);
            background: rgba(255,255,255,0.3); backdrop-filter: blur(4px);
            border-radius: 14px; color: #f97316;
            font-size: 14px; font-weight: 600; cursor: pointer;
            display: flex; align-items: center; justify-content: center; gap: 6px;
            transition: all 0.2s ease;
        }
        .add-btn:hover {
            border-color: #f97316; color: #fff; background: #f97316;
            box-shadow: 0 4px 16px rgba(249,115,22,0.25);
        }
        .add-btn svg { width: 18px; height: 18px; }

        .log-section { margin-bottom: 14px; }
        .log-header {
            display: flex; align-items: center; justify-content: space-between;
            padding: 14px 18px; cursor: pointer; user-select: none;
        }
        .log-header-left { display: flex; align-items: center; gap: 10px; }
        .log-header-left svg { width: 18px; height: 18px; color: #f97316; }
        .log-header-left span { font-size: 15px; font-weight: 600; color: #374151; }
        .log-arrow {
            width: 24px; height: 24px; color: #9ca3af;
            transition: transform 0.3s cubic-bezier(0.32,0.72,0,1);
        }
        .log-arrow.open { transform: rotate(180deg); }
        .log-body {
            max-height: 0; overflow: hidden;
            transition: max-height 0.4s cubic-bezier(0.32,0.72,0,1), padding 0.3s ease;
            padding: 0 18px;
        }
        .log-body.open {
            max-height: 240px; padding: 0 18px 18px;
        }
        .log-box {
            background: rgba(255,255,255,0.35); backdrop-filter: blur(8px);
            border: 1px solid rgba(255,255,255,0.4); border-radius: 12px;
            padding: 12px;
            font-size: 12px; color: #6b7280; line-height: 1.8;
            max-height: 200px; overflow-y: auto;
            font-family: 'SF Mono', 'Consolas', monospace;
        }

        .empty-state { text-align: center; padding: 24px; color: #9ca3af; font-size: 13px; }

        .modal-overlay {
            position: fixed; inset: 0; background: rgba(0,0,0,0.35);
            backdrop-filter: blur(6px); display: none;
            align-items: flex-end; justify-content: center;
            z-index: 100; opacity: 0; transition: opacity 0.3s;
        }
        .modal-overlay.show { display: flex; opacity: 1; }
        .modal {
            background: linear-gradient(180deg, rgba(255,255,255,0.85) 0%, rgba(255,248,240,0.9) 100%);
            backdrop-filter: blur(28px) saturate(1.5);
            -webkit-backdrop-filter: blur(28px) saturate(1.5);
            border: 1px solid rgba(255,255,255,0.7);
            width: 100%; max-width: 420px;
            border-radius: 24px 24px 0 0; padding: 24px 20px 28px;
            transform: translateY(100%);
            transition: transform 0.35s cubic-bezier(0.32,0.72,0,1);
            box-shadow: 0 -4px 32px rgba(0,0,0,0.06), inset 0 1px 0 rgba(255,255,255,0.8);
        }
        .modal-overlay.show .modal { transform: translateY(0); }
        .modal-header {
            display: flex; align-items: center; justify-content: space-between;
            margin-bottom: 24px;
        }
        .modal-header h3 { font-size: 18px; font-weight: 600; color: #1f2937; }
        .modal-close {
            width: 36px; height: 36px; border: none;
            background: rgba(243,244,246,0.6); backdrop-filter: blur(4px);
            border-radius: 50%; cursor: pointer;
            display: flex; align-items: center; justify-content: center;
            color: #6b7280; transition: all 0.2s;
        }
        .modal-close:hover { background: rgba(249,115,22,0.1); color: #f97316; }
        .modal-close svg { width: 18px; height: 18px; }

        .time-picker-wrap { margin-bottom: 24px; }
        .ampm-toggle {
            display: flex; justify-content: center; margin-bottom: 16px;
        }
        .ampm-btn {
            padding: 6px 20px; border: 1.5px solid rgba(229,231,235,0.6);
            background: rgba(255,255,255,0.4); backdrop-filter: blur(4px);
            font-size: 14px; font-weight: 500; color: #6b7280; cursor: pointer;
            transition: all 0.2s;
        }
        .ampm-btn:first-child { border-radius: 10px 0 0 10px; border-right: none; }
        .ampm-btn:last-child { border-radius: 0 10px 10px 0; }
        .ampm-btn.active {
            background: linear-gradient(135deg, #f97316 0%, #ea580c 100%);
            color: white; border-color: #f97316;
        }

        /* ===== 新版时间轮盘 - 苹果风格 ===== */
        .time-picker {
            display: flex; align-items: center; justify-content: center;
            gap: 4px; position: relative; height: 200px;
        }
        .wheel-container {
            width: 80px; height: 200px;
            overflow: hidden; position: relative;
            background: rgba(255,255,255,0.3);
            border-radius: 16px; border: 1px solid rgba(255,255,255,0.5);
            touch-action: none;
            -webkit-touch-callout: none;
            -webkit-user-select: none;
            user-select: none;
        }
        /* 顶部和底部遮罩 */
        .wheel-container::before, .wheel-container::after {
            content: ''; position: absolute; left: 0; right: 0; height: 80px; z-index: 2; pointer-events: none;
        }
        .wheel-container::before {
            top: 0;
            background: linear-gradient(to bottom, rgba(255,255,255,0.98) 0%, rgba(255,255,255,0.6) 40%, transparent 100%);
            border-radius: 16px 16px 0 0;
        }
        .wheel-container::after {
            bottom: 0;
            background: linear-gradient(to top, rgba(255,255,255,0.98) 0%, rgba(255,255,255,0.6) 40%, transparent 100%);
            border-radius: 0 0 16px 16px;
        }
        .wheel-scroll {
            height: 100%;
            overflow-y: scroll;
            overflow-x: hidden;
            scroll-snap-type: y mandatory;
            -webkit-overflow-scrolling: touch;
            scrollbar-width: none;
            -ms-overflow-style: none;
            position: relative;
            z-index: 0;
        }
        .wheel-scroll::-webkit-scrollbar { display: none; }
        .wheel-item {
            height: 40px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 20px;
            font-weight: 500;
            color: #9ca3af;
            font-variant-numeric: tabular-nums;
            scroll-snap-align: center;
            scroll-snap-stop: always;
            transition: color 0.15s, font-size 0.15s, font-weight 0.15s;
        }
        .wheel-item.selected {
            color: #1f2937;
            font-size: 24px;
            font-weight: 700;
        }
        .time-sep-wrap {
            display: flex; align-items: center; justify-content: center;
            width: 30px; height: 200px;
        }
        .time-sep {
            font-size: 28px; font-weight: 700; color: #9ca3af;
        }

        .form-row {
            display: flex; align-items: center; margin-bottom: 18px;
            padding: 0 4px;
        }
        .form-label {
            font-size: 15px; font-weight: 500; color: #374151;
            width: 56px; flex-shrink: 0;
        }

        .custom-dropdown { position: relative; flex: 1; }
        .dropdown-trigger {
            width: 100%; padding: 12px 14px;
            background: rgba(255,255,255,0.5); backdrop-filter: blur(4px);
            border: 1.5px solid rgba(229,231,235,0.6); border-radius: 12px;
            font-size: 14px; font-weight: 500; color: #374151;
            display: flex; align-items: center; justify-content: space-between;
            cursor: pointer; outline: none; transition: all 0.2s; user-select: none;
        }
        .dropdown-trigger:focus {
            border-color: #f97316;
            box-shadow: 0 0 0 3px rgba(249,115,22,0.1);
        }
        .dropdown-trigger svg { width: 16px; height: 16px; color: #9ca3af; transition: transform 0.2s; }
        .dropdown-trigger.open svg { transform: rotate(180deg); }
        .dropdown-menu {
            position: absolute; top: calc(100% + 6px); left: 0; right: 0;
            background: rgba(255,255,255,0.85); backdrop-filter: blur(20px) saturate(1.4);
            -webkit-backdrop-filter: blur(20px) saturate(1.4);
            border: 1px solid rgba(255,255,255,0.65); border-radius: 14px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.08);
            max-height: 0; overflow: hidden; opacity: 0;
            transition: opacity 0.25s cubic-bezier(0.32,0.72,0,1), max-height 0.25s cubic-bezier(0.32,0.72,0,1);
            z-index: 10;
        }
        .dropdown-menu.open { max-height: 140px; opacity: 1; padding: 6px 0; overflow-y: auto; -webkit-overflow-scrolling: touch; }
        .dropdown-menu::-webkit-scrollbar { width: 4px; }
        .dropdown-menu::-webkit-scrollbar-track { background: transparent; }
        .dropdown-menu::-webkit-scrollbar-thumb { background: rgba(156,163,175,0.4); border-radius: 4px; }
        .dropdown-item {
            padding: 10px 14px; font-size: 14px; font-weight: 500; color: #374151;
            cursor: pointer; transition: all 0.15s; margin: 0 6px; border-radius: 10px;
        }
        .dropdown-item:hover { background: rgba(249,115,22,0.08); color: #f97316; }
        .dropdown-item.selected { background: rgba(249,115,22,0.12); color: #f97316; }

        .action-btns { display: flex; gap: 10px; flex: 1; }
        .action-btn {
            flex: 1; padding: 12px;
            border: 1.5px solid rgba(229,231,235,0.6);
            background: rgba(255,255,255,0.4); backdrop-filter: blur(4px);
            border-radius: 12px; font-size: 14px;
            color: #6b7280; cursor: pointer; transition: all 0.2s; font-weight: 500;
        }
        .action-btn.active {
            border-color: #f97316; color: #f97316;
            background: rgba(249,115,22,0.08);
            box-shadow: 0 2px 8px rgba(249,115,22,0.1);
        }

        .confirm-btn {
            width: 100%; padding: 15px;
            background: linear-gradient(135deg, #f97316 0%, #ea580c 100%);
            color: white; border: none; border-radius: 14px;
            font-size: 16px; font-weight: 600; cursor: pointer;
            transition: all 0.2s; margin-top: 8px;
            box-shadow: 0 4px 16px rgba(249,115,22,0.25);
        }
        .confirm-btn:hover { background: linear-gradient(135deg, #ea580c 0%, #c2410c 100%); transform: translateY(-1px); }
        .confirm-btn:active { transform: scale(0.98); }
        .confirm-btn:disabled {
            opacity: 0.6; cursor: not-allowed;
            background: linear-gradient(135deg, #d1d5db 0%, #9ca3af 100%);
            box-shadow: none;
        }
    </style>
</head>
<body>
    <div id="app">
        <div class="header">
            <div class="header-left">
                <h1>智能灯控</h1>
                <div class="sub">ESP-01S</div>
            </div>
        </div>

        <div class="frost-card power-card" id="powerCard">
            <button class="power-btn" id="powerBtn" onclick="togglePower()">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                    <path d="M18.36 6.64a9 9 0 1 1-12.73 0"/>
                    <line x1="12" y1="2" x2="12" y2="12"/>
                </svg>
            </button>
            <p class="power-text" id="powerText">加载中...</p>
            <p class="power-sub" id="powerSub">--</p>
        </div>

        <div class="frost-card section">
            <div class="section-header">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                    <circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>
                </svg>
                <span>定时任务</span>
                <span class="count" id="timerCount">0个</span>
            </div>
            <div class="timer-list" id="timerList"></div>
            <button class="add-btn" onclick="openModal()">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                    <line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>
                </svg>
                添加定时
            </button>
        </div>

        <div class="frost-card log-section">
            <div class="log-header" onclick="toggleLog()">
                <div class="log-header-left">
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                        <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/>
                        <polyline points="14 2 14 8 20 8"/><line x1="16" y1="13" x2="8" y2="13"/>
                        <line x1="16" y1="17" x2="8" y2="17"/><polyline points="10 9 9 9 8 9"/>
                    </svg>
                    <span>日志</span>
                </div>
                <svg class="log-arrow" id="logArrow" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                    <polyline points="6 9 12 15 18 9"/>
                </svg>
            </div>
            <div class="log-body" id="logBody">
                <div class="log-box" id="logBox"></div>
            </div>
        </div>
    </div>

    <div class="modal-overlay" id="modalOverlay" onclick="closeModal(event)">
        <div class="modal" onclick="event.stopPropagation()">
            <div class="modal-header">
                <h3>添加定时任务</h3>
                <button class="modal-close" onclick="closeModal()">
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                        <line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>
                    </svg>
                </button>
            </div>

            <div class="time-picker-wrap">
                <div class="ampm-toggle">
                    <button class="ampm-btn active" id="btnAM" onclick="setAmPm('AM')">上午</button>
                    <button class="ampm-btn" id="btnPM" onclick="setAmPm('PM')">下午</button>
                </div>
                <div class="time-picker">
                    <div class="wheel-container" id="hourWheel">
                        <div class="wheel-scroll" id="hourScroll"></div>
                    </div>
                    <div class="time-sep-wrap">
                        <span class="time-sep">:</span>
                    </div>
                    <div class="wheel-container" id="minuteWheel">
                        <div class="wheel-scroll" id="minuteScroll"></div>
                    </div>
                </div>
            </div>

            <div class="form-row">
                <span class="form-label">重复</span>
                <div class="custom-dropdown" id="weekdayDropdown">
                    <div class="dropdown-trigger" id="weekdayTrigger" onclick="toggleDropdown()">
                        <span id="weekdayText">每天</span>
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                            <polyline points="6 9 12 15 18 9"/>
                        </svg>
                    </div>
                    <div class="dropdown-menu" id="weekdayMenu">
                        <div class="dropdown-item selected" data-value="127" onclick="selectWeekday(127)">每天</div>
                        <div class="dropdown-item" data-value="1" onclick="selectWeekday(1)">周一</div>
                        <div class="dropdown-item" data-value="2" onclick="selectWeekday(2)">周二</div>
                        <div class="dropdown-item" data-value="4" onclick="selectWeekday(4)">周三</div>
                        <div class="dropdown-item" data-value="8" onclick="selectWeekday(8)">周四</div>
                        <div class="dropdown-item" data-value="16" onclick="selectWeekday(16)">周五</div>
                        <div class="dropdown-item" data-value="32" onclick="selectWeekday(32)">周六</div>
                        <div class="dropdown-item" data-value="64" onclick="selectWeekday(64)">周日</div>
                        <div class="dropdown-item" data-value="62" onclick="selectWeekday(62)">工作日</div>
                        <div class="dropdown-item" data-value="65" onclick="selectWeekday(65)">周末</div>
                        <div class="dropdown-item" data-value="0" onclick="selectWeekday(0)">仅一次</div>
                    </div>
                </div>
            </div>

            <div class="form-row">
                <span class="form-label">动作</span>
                <div class="action-btns">
                    <button class="action-btn active" id="actOn" onclick="setAction('on')">开灯</button>
                    <button class="action-btn" id="actOff" onclick="setAction('off')">关灯</button>
                </div>
            </div>

            <button class="confirm-btn" id="confirmBtn" onclick="addTimer()">确定</button>
        </div>
    </div>

    <script>
        let powerOn = false;
        let timers = [];
        let selectedAction = 'on';
        let ampm = 'AM';
        let tempHour = 8, tempMinute = 0;
        let selectedDaysMask = 127;
        let logOpen = false;
        let isAddingTimer = false;

        const WEEKDAY_NAMES = {
            127: '每天', 1: '周一', 2: '周二', 4: '周三', 8: '周四',
            16: '周五', 32: '周六', 64: '周日', 62: '工作日', 65: '周末', 0: '仅一次'
        };

        // 轮盘配置
        const ITEM_H = 40;
        const VISIBLE_ITEMS = 5; // 可见项数（奇数）
        const PADDING_ITEMS = 20; // 每侧重复组数（假循环用）

        function log(msg) {
            const box = document.getElementById('logBox');
            const time = new Date().toLocaleTimeString('zh-CN', {hour:'2-digit',minute:'2-digit',second:'2-digit'});
            box.innerHTML = '<div>[' + time + '] ' + msg + '</div>' + box.innerHTML;
        }

        function toggleLog() {
            logOpen = !logOpen;
            document.getElementById('logBody').classList.toggle('open', logOpen);
            document.getElementById('logArrow').classList.toggle('open', logOpen);
        }

        function updatePowerUI() {
            const btn = document.getElementById('powerBtn');
            const card = document.getElementById('powerCard');
            const text = document.getElementById('powerText');
            const sub = document.getElementById('powerSub');
            if (powerOn) {
                btn.classList.add('on');
                card.classList.add('active');
                text.innerText = '已开启'; text.style.color = '#f97316';
                sub.innerText = '点击关闭';
            } else {
                btn.classList.remove('on');
                card.classList.remove('active');
                text.innerText = '已关闭'; text.style.color = '#374151';
                sub.innerText = '点击开启';
            }
        }

        function togglePower() {
            fetch('/toggle').then(r => r.text()).then(() => {
                updateStatus();
            }).catch(() => {});
        }

        function updateStatus() {
            fetch('/status').then(r => r.text()).then(t => {
                powerOn = t.trim() === '1';
                updatePowerUI();
            }).catch(() => {
                document.getElementById('powerText').innerText = '离线';
                document.getElementById('powerSub').innerText = '检查连接';
            });
        }

        function getDaysText(daysMask) {
            if (WEEKDAY_NAMES[daysMask]) return WEEKDAY_NAMES[daysMask];
            const names = ['周日','周一','周二','周三','周四','周五','周六'];
            let list = [];
            for (let i = 0; i < 7; i++) {
                if (daysMask & (1 << i)) list.push(names[i]);
            }
            return list.join(' ');
        }

        function renderTimers() {
            const list = document.getElementById('timerList');
            document.getElementById('timerCount').innerText = timers.length + '个';
            if (timers.length === 0) {
                list.innerHTML = '<div class="empty-state">暂无定时任务，点击添加</div>';
                return;
            }
            list.innerHTML = timers.map((t, i) => {
                return '<div class="timer-item">' +
                    '<div class="timer-time">' + String(t.hour).padStart(2,'0') + ':' + String(t.minute).padStart(2,'0') + '</div>' +
                    '<div class="timer-info">' +
                        '<div class="timer-days">' + getDaysText(t.days) + '</div>' +
                        '<div class="timer-action ' + t.action + '">' + (t.action==='on'?'开灯':'关灯') + '</div>' +
                    '</div>' +
                    '<div class="timer-switch ' + (t.enabled?'active':'') + '" onclick="toggleTimer(' + i + ')"></div>' +
                    '<button class="timer-delete" onclick="deleteTimer(' + i + ')">' +
                        '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">' +
                            '<polyline points="3 6 5 6 21 6"/>' +
                            '<path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/>' +
                        '</svg>' +
                    '</button>' +
                '</div>';
            }).join('');
        }

        function toggleTimer(idx) {
            timers[idx].enabled = !timers[idx].enabled;
            saveTimers();
            renderTimers();
            log('定时 #' + (idx+1) + ' ' + (timers[idx].enabled?'启用':'禁用'));
        }

        function deleteTimer(idx) {
            timers.splice(idx, 1);
            saveTimers();
            renderTimers();
            log('删除定时任务');
        }

        // ========== 苹果风格假循环轮盘 ==========
        // 核心原理：
        // 1. 在首尾各添加多组重复数据（PADDING_ITEMS组）
        // 2. 使用 CSS scroll-snap-type: y mandatory + scroll-snap-align: center 让选中项自动吸附居中
        // 3. 监听 scroll 事件，当滚动到边界区域时，静默重置到中间对应位置
        // 4. 使用 -webkit-overflow-scrolling: touch 实现原生惯性滚动

        function buildWheel(type, initialValue) {
            const scroll = document.getElementById(type + 'Scroll');
            const count = type === 'hour' ? 12 : 60;
            let html = '';

            // 顶部 padding（空白占位，让第一项能滚动到中间）
            html += '<div class="wheel-item" style="height:' + (ITEM_H * Math.floor(VISIBLE_ITEMS/2)) + 'px;scroll-snap-align:none;"></div>';

            // 前置重复数据（假循环）
            for (let rep = 0; rep < PADDING_ITEMS; rep++) {
                for (let i = 0; i < count; i++) {
                    let display = type === 'hour' ? (i === 0 ? 12 : i) : i;
                    html += '<div class="wheel-item" data-value="' + i + '">' + String(display).padStart(2,'0') + '</div>';
                }
            }

            // 中间真实数据（初始选中值在这里）
            for (let i = 0; i < count; i++) {
                let display = type === 'hour' ? (i === 0 ? 12 : i) : i;
                html += '<div class="wheel-item" data-value="' + i + '">' + String(display).padStart(2,'0') + '</div>';
            }

            // 后置重复数据（假循环）
            for (let rep = 0; rep < PADDING_ITEMS; rep++) {
                for (let i = 0; i < count; i++) {
                    let display = type === 'hour' ? (i === 0 ? 12 : i) : i;
                    html += '<div class="wheel-item" data-value="' + i + '">' + String(display).padStart(2,'0') + '</div>';
                }
            }

            // 底部 padding
            html += '<div class="wheel-item" style="height:' + (ITEM_H * Math.floor(VISIBLE_ITEMS/2)) + 'px;scroll-snap-align:none;"></div>';

            scroll.innerHTML = html;

            // 计算初始滚动位置：让 initialValue 位于中间那组真实数据中
            // 每组 count 个数据，前面有 PADDING_ITEMS 组 + 顶部 padding
            const topPaddingH = ITEM_H * Math.floor(VISIBLE_ITEMS/2);
            const groupStart = topPaddingH + PADDING_ITEMS * count * ITEM_H;
            const targetScroll = groupStart + initialValue * ITEM_H;

            // 先设置 scrollTop，再绑定事件
            requestAnimationFrame(() => {
                scroll.scrollTop = targetScroll;
                updateWheelSelection(type);
            });

            // 绑定滚动事件
            scroll.addEventListener('scroll', function() {
                handleWheelScroll(type);
            }, { passive: true });

            // 绑定 scrollend 事件（用于处理边界重置）
            scroll.addEventListener('scrollend', function() {
                handleWheelScrollEnd(type);
            });
            // 兼容没有 scrollend 的浏览器
            let scrollTimeout;
            scroll.addEventListener('scroll', function() {
                clearTimeout(scrollTimeout);
                scrollTimeout = setTimeout(function() {
                    handleWheelScrollEnd(type);
                }, 150);
            }, { passive: true });
        }

        function handleWheelScroll(type) {
            updateWheelSelection(type);
        }

        function handleWheelScrollEnd(type) {
            const scroll = document.getElementById(type + 'Scroll');
            const count = type === 'hour' ? 12 : 60;
            const topPaddingH = ITEM_H * Math.floor(VISIBLE_ITEMS/2);
            const oneGroupH = count * ITEM_H;
            const totalRepeatH = PADDING_ITEMS * oneGroupH;
            const middleStart = topPaddingH + totalRepeatH;
            const middleEnd = middleStart + oneGroupH;

            // 获取当前选中的值
            const items = scroll.querySelectorAll('.wheel-item[data-value]');
            let currentVal = type === 'hour' ? tempHour : tempMinute;
            for (let item of items) {
                if (item.classList.contains('selected')) {
                    currentVal = parseInt(item.getAttribute('data-value'));
                    break;
                }
            }

            // 如果滚动到边界区域，静默重置到中间区域
            if (scroll.scrollTop < middleStart - oneGroupH * 0.5) {
                // 滚到了前置区域，重置到中间区域对应位置
                const newScroll = middleStart + currentVal * ITEM_H;
                scroll.style.scrollBehavior = 'auto';
                scroll.scrollTop = newScroll;
                scroll.style.scrollBehavior = 'smooth';
            } else if (scroll.scrollTop > middleEnd + oneGroupH * 0.5) {
                // 滚到了后置区域，重置到中间区域对应位置
                const newScroll = middleStart + currentVal * ITEM_H;
                scroll.style.scrollBehavior = 'auto';
                scroll.scrollTop = newScroll;
                scroll.style.scrollBehavior = 'smooth';
            }

            // 更新状态变量
            if (type === 'hour') tempHour = currentVal;
            else tempMinute = currentVal;
        }

        function updateWheelSelection(type) {
            const scroll = document.getElementById(type + 'Scroll');
            const container = scroll.parentElement;
            const containerRect = container.getBoundingClientRect();
            const centerY = containerRect.top + containerRect.height / 2;

            const items = scroll.querySelectorAll('.wheel-item[data-value]');
            let closestItem = null;
            let closestDist = Infinity;

            items.forEach(item => {
                const rect = item.getBoundingClientRect();
                const itemCenterY = rect.top + rect.height / 2;
                const dist = Math.abs(itemCenterY - centerY);
                if (dist < closestDist) {
                    closestDist = dist;
                    closestItem = item;
                }
                item.classList.remove('selected');
            });

            if (closestItem) {
                closestItem.classList.add('selected');
                const val = parseInt(closestItem.getAttribute('data-value'));
                if (type === 'hour') tempHour = val;
                else tempMinute = val;
            }
        }

        function setAmPm(val) {
            ampm = val;
            document.getElementById('btnAM').classList.toggle('active', val === 'AM');
            document.getElementById('btnPM').classList.toggle('active', val === 'PM');
        }

        // ========== Modal ==========
        function openModal() {
            tempHour = 8; tempMinute = 0; ampm = 'AM'; selectedAction = 'on'; selectedDaysMask = 127;
            setAmPm('AM');
            buildWheel('hour', tempHour);
            buildWheel('minute', tempMinute);
            setAction('on');
            selectWeekdayUI(127);
            isAddingTimer = false;
            const btn = document.getElementById('confirmBtn');
            btn.disabled = false; btn.innerText = '确定';
            document.getElementById('modalOverlay').classList.add('show');
        }

        function closeModal(e) {
            if (e && e.target !== document.getElementById('modalOverlay')) return;
            document.getElementById('modalOverlay').classList.remove('show');
            closeDropdown();
        }

        function setAction(act) {
            selectedAction = act;
            document.getElementById('actOn').classList.toggle('active', act === 'on');
            document.getElementById('actOff').classList.toggle('active', act === 'off');
        }

        function get24Hour() {
            let h = tempHour;
            if (h === 0) h = 12;
            if (ampm === 'PM' && h !== 12) h += 12;
            if (ampm === 'AM' && h === 12) h = 0;
            return h;
        }

        // ========== 去重检查 ==========
        function isDuplicateTimer(h24, minute, days, action) {
            for (let i = 0; i < timers.length; i++) {
                if (timers[i].hour === h24 &&
                    timers[i].minute === minute &&
                    timers[i].days === days &&
                    timers[i].action === action) {
                    return true;
                }
            }
            return false;
        }

        function addTimer() {
            if (isAddingTimer) return;
            isAddingTimer = true;
            const btn = document.getElementById('confirmBtn');
            btn.disabled = true; btn.innerText = '添加中...';

            const h24 = get24Hour();

            // 检查重复
            if (isDuplicateTimer(h24, tempMinute, selectedDaysMask, selectedAction)) {
                btn.disabled = false;
                btn.innerText = '确定';
                isAddingTimer = false;
                log('该定时任务已存在');
                return;
            }

            timers.push({
                hour: h24, minute: tempMinute,
                days: selectedDaysMask, action: selectedAction, enabled: true
            });
            saveTimers();
            renderTimers();
            closeModal();
            log('添加定时 ' + String(h24).padStart(2,'0') + ':' + String(tempMinute).padStart(2,'0') + ' ' + getDaysText(selectedDaysMask) + ' ' + (selectedAction==='on'?'开灯':'关灯'));

            setTimeout(() => { isAddingTimer = false; }, 500);
        }

        function saveTimers() {
            fetch('/timers', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(timers)
            }).catch(() => {});
        }

        function loadTimers() {
            fetch('/timers').then(r => r.json()).then(data => {
                timers = data || [];
                renderTimers();
            }).catch(() => { renderTimers(); });
        }

        // ========== Custom Dropdown ==========
        function toggleDropdown() {
            const menu = document.getElementById('weekdayMenu');
            const trigger = document.getElementById('weekdayTrigger');
            const isOpen = menu.classList.contains('open');
            if (isOpen) { closeDropdown(); }
            else { menu.classList.add('open'); trigger.classList.add('open'); }
        }

        function closeDropdown() {
            document.getElementById('weekdayMenu').classList.remove('open');
            document.getElementById('weekdayTrigger').classList.remove('open');
        }

        function selectWeekday(mask) {
            selectedDaysMask = mask;
            selectWeekdayUI(mask);
            closeDropdown();
        }

        function selectWeekdayUI(mask) {
            document.getElementById('weekdayText').innerText = WEEKDAY_NAMES[mask] || getDaysText(mask);
            document.querySelectorAll('.dropdown-item').forEach(el => {
                el.classList.toggle('selected', parseInt(el.getAttribute('data-value')) === mask);
            });
        }

        document.addEventListener('click', function(e) {
            const dd = document.getElementById('weekdayDropdown');
            if (dd && !dd.contains(e.target)) closeDropdown();
        });

        setInterval(updateStatus, 2000);
        updateStatus();
        loadTimers();
        log('系统已连接');
    </script>
</body>
</html>)rawliteral";

// ========== 简易 JSON 解析（无外部库） ==========

const char* skipSpace(const char* p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    return p;
}

const char* parseBool(const char* p, bool& out) {
    p = skipSpace(p);
    if (strncmp(p, "true", 4) == 0) { out = true; return p + 4; }
    if (strncmp(p, "false", 5) == 0) { out = false; return p + 5; }
    return p;
}

const char* parseInt(const char* p, int& out) {
    p = skipSpace(p);
    out = 0;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    while (*p >= '0' && *p <= '9') {
        out = out * 10 + (*p - '0');
        p++;
    }
    if (neg) out = -out;
    return p;
}

const char* parseStringVal(const char* p, char* out, int maxLen) {
    p = skipSpace(p);
    if (*p != '"') return p;
    p++;
    int i = 0;
    while (*p != '"' && *p != '\0' && i < maxLen - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    if (*p == '"') p++;
    return p;
}

void parseTimersFromBody(const String& body) {
    const char* p = body.c_str();
    timerCount = 0;
    p = skipSpace(p);
    if (*p != '[') return;
    p++;
    while (true) {
        p = skipSpace(p);
        if (*p == ']') break;
        if (*p != '{') { p++; continue; }
        p++;
        if (timerCount >= 10) break;
        TimerTask& t = timers[timerCount];
        t.hour = 0; t.minute = 0; t.days = 127; t.action = true; t.enabled = true; t.executed = false;
        while (true) {
            p = skipSpace(p);
            if (*p == '}') { p++; break; }
            char key[16];
            p = parseStringVal(p, key, 16);
            p = skipSpace(p);
            if (*p == ':') p++;
            p = skipSpace(p);
            if (strcmp(key, "hour") == 0) {
                int v; p = parseInt(p, v); t.hour = (uint8_t)v;
            } else if (strcmp(key, "minute") == 0) {
                int v; p = parseInt(p, v); t.minute = (uint8_t)v;
            } else if (strcmp(key, "days") == 0) {
                int v; p = parseInt(p, v); t.days = (uint8_t)v;
            } else if (strcmp(key, "action") == 0) {
                char val[8];
                p = parseStringVal(p, val, 8);
                t.action = (strcmp(val, "on") == 0);
            } else if (strcmp(key, "enabled") == 0) {
                p = parseBool(p, t.enabled);
            } else {
                if (*p == '"') { char buf[32]; p = parseStringVal(p, buf, 32); }
                else if (*p == 't' || *p == 'f') { bool b; p = parseBool(p, b); }
                else { int v; p = parseInt(p, v); }
            }
            p = skipSpace(p);
            if (*p == ',') p++;
        }
        timerCount++;
        p = skipSpace(p);
        if (*p == ',') p++;
    }
}

// ========== HTTP 处理器 ==========

void handleRoot() {
    server.send(200, "text/html", htmlPage);
}

void handleToggle() {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? LOW : HIGH);
    server.send(200, "text/plain", ledState ? "1" : "0");
}

void handleStatus() {
    server.send(200, "text/plain", ledState ? "1" : "0");
}

void handleGetTimers() {
    String json = "[";
    for (int i = 0; i < timerCount; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"hour\":" + String(timers[i].hour) + ",";
        json += "\"minute\":" + String(timers[i].minute) + ",";
        json += "\"days\":" + String(timers[i].days) + ",";
        json += "\"action\":\"" + String(timers[i].action ? "on" : "off") + "\",";
        json += "\"enabled\":" + String(timers[i].enabled ? "true" : "false");
        json += "}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handlePostTimers() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Bad Request");
        return;
    }
    parseTimersFromBody(server.arg("plain"));
    server.send(200, "text/plain", "OK");
}

// ========== 定时任务检查 ==========

unsigned long lastTimerCheck = 0;
int lastCheckedMinute = -1;

void checkTimers() {
    unsigned long now = millis();
    if (now - lastTimerCheck < 1000) return;
    lastTimerCheck = now;

    time_t nowTime = time(nullptr);
    struct tm* timeinfo = localtime(&nowTime);
    if (!timeinfo) return;

    int currentHour = timeinfo->tm_hour;
    int currentMinute = timeinfo->tm_min;
    int currentSecond = timeinfo->tm_sec;
    int wday = timeinfo->tm_wday; // 0=周日, 1=周一...

    if (currentSecond != 0) return;
    if (currentMinute == lastCheckedMinute) return;
    lastCheckedMinute = currentMinute;

    for (int i = 0; i < timerCount; i++) {
        if (!timers[i].enabled) continue;
        if (timers[i].hour != currentHour || timers[i].minute != currentMinute) continue;

        uint8_t days = timers[i].days;
        if (days == 0) {
            if (timers[i].executed) continue;
        } else {
            int bit = (wday == 0) ? 6 : (wday - 1);
            if (!(days & (1 << bit))) continue;
        }

        ledState = timers[i].action;
        digitalWrite(LED_PIN, ledState ? LOW : HIGH);

        if (days == 0) {
            timers[i].executed = true;
        }

        Serial.printf("[Timer] %02d:%02d %s days=%d wday=%d\n",
            timers[i].hour, timers[i].minute,
            timers[i].action ? "ON" : "OFF",
            days, wday);
    }
}

// ========== Setup & Loop ==========

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("BOOT");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());

    configTime(8 * 3600, 0, "ntp.aliyun.com", "ntp1.aliyun.com");
    Serial.println("NTP syncing...");
    delay(2000);

    server.on("/", handleRoot);
    server.on("/toggle", handleToggle);
    server.on("/status", handleStatus);
    server.on("/timers", HTTP_GET, handleGetTimers);
    server.on("/timers", HTTP_POST, handlePostTimers);
    server.begin();

    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
    checkTimers();
}