#pragma once
#include <Arduino.h>

// Dashboard. Ranges and colours of the level displays:
//   RF    0..150 W, warn from 100, high from 120
//   VSWR  1..3,     warn from 1.5, high from 2
//   temp  20..80,   warn from 50,  high from 60
//   normal #00b33c, warn #ff9900, high #e60000, unlit #595959
//
// All wording lives in the object L (de/en). For hints the firmware sends only
// a code plus an argument, so nothing stays hard-wired on that side.
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>JUMA PA-100D</title><style>
:root{--bg:#111;--card:#1e2228;--card2:#191d23;--line:#3a4049;--fg:#eee;--dim:#8b95a3;
--ok:#00b33c;--warn:#ff9900;--bad:#e60000;--off:#595959;--acc:#0eb8c0;
--btn:#272c34;--btn2:#2a2f37;--sw:#3a4049}
/* Light: same semantics, but the signal colours slightly darkened - #ff9900
   is barely readable as text on white. */
:root[data-theme="light"]{--bg:#eef1f5;--card:#fff;--card2:#f5f7fa;--line:#d2d8e0;
--fg:#161a1f;--dim:#5c6674;--ok:#079c35;--warn:#c97a00;--bad:#cc1f1a;--off:#ccd2da;
--acc:#0a7b82;--btn:#e7ebf0;--btn2:#dfe4ea;--sw:#b9c1cb}
@media (prefers-color-scheme: light){
  :root:not([data-theme="dark"]){--bg:#eef1f5;--card:#fff;--card2:#f5f7fa;--line:#d2d8e0;
  --fg:#161a1f;--dim:#5c6674;--ok:#079c35;--warn:#c97a00;--bad:#cc1f1a;--off:#ccd2da;
  --acc:#0a7b82;--btn:#e7ebf0;--btn2:#dfe4ea;--sw:#b9c1cb}
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);
font:15px/1.45 -apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica Neue,sans-serif}
.wrap{max-width:1240px;margin:0 auto;padding:16px}
header{display:flex;align-items:center;gap:14px;flex-wrap:wrap;margin-bottom:14px}
h1{font-size:18px;margin:0;font-weight:600;letter-spacing:.3px}
.dot{width:9px;height:9px;border-radius:50%;display:inline-block;background:var(--bad);flex:0 0 auto}
.dot.on{background:var(--ok)}
.pill{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--dim)}
.hbtn{background:var(--btn);border:1px solid var(--line);color:var(--fg);border-radius:8px;
height:38px;min-width:38px;padding:0 10px;font-size:13px;cursor:pointer;flex:0 0 auto;font-family:inherit}
.hbtn:hover{border-color:var(--acc)}
#bGear{font-size:17px;padding:0}
.seg,.lang{display:flex}
.seg .hbtn{border-radius:0;margin-left:-1px;font-weight:600;color:var(--dim);height:34px;font-size:13px}
.seg .hbtn:first-child{border-radius:7px 0 0 7px;margin-left:0}
.seg .hbtn:last-child{border-radius:0 7px 7px 0}
.seg .hbtn.act{background:var(--acc);border-color:var(--acc);color:#08191b}
.sellb{font-size:12px;color:var(--dim)}
.lang .hbtn{border-radius:0;margin-left:-1px;font-weight:600;color:var(--dim)}
.lang .hbtn:first-child{border-radius:8px 0 0 8px;margin-left:0}
.lang .hbtn:last-child{border-radius:0 8px 8px 0}
.lang .hbtn.act{background:var(--acc);border-color:var(--acc);color:#08191b}
code{font-family:ui-monospace,Menlo,monospace;font-size:11px;color:var(--dim);word-break:break-all}

.cols{display:grid;grid-template-columns:1fr;gap:12px}
@media(min-width:900px){.cols{grid-template-columns:minmax(0,1fr) minmax(0,1.1fr)}}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:12px}
.card h2{font-size:11px;text-transform:uppercase;letter-spacing:.9px;color:var(--acc);margin:0 0 12px}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
button{background:var(--btn);color:var(--fg);border:1px solid var(--line);border-radius:7px;
padding:9px 14px;font-size:14px;cursor:pointer;font-family:inherit}
button:hover{border-color:var(--acc)}
button:disabled{opacity:.38;cursor:not-allowed}
button:disabled:hover{border-color:var(--line)}
button.act{background:var(--acc);border-color:var(--acc);color:#08191b;font-weight:600}
button.op{background:var(--ok);border-color:var(--ok);color:#03170a;font-weight:600}
button.danger{border-color:#6b2320;color:#f0928d}
.state{font-size:30px;font-weight:700;letter-spacing:1px;line-height:1}
.txb{padding:4px 12px;border-radius:5px;font-size:13px;font-weight:700;background:var(--btn2);color:var(--dim)}
.txb.on{background:var(--bad);color:#fff}
.bands{display:grid;grid-template-columns:repeat(auto-fit,minmax(66px,1fr));gap:8px}

.lvl{margin-bottom:14px}
.lvl:last-child{margin-bottom:0}
.lvl .hd{display:flex;align-items:baseline;gap:8px;margin-bottom:5px}
.lvl .lb{font-size:11px;text-transform:uppercase;letter-spacing:.8px;color:var(--dim)}
.lvl .vl{margin-left:auto;font-size:22px;font-variant-numeric:tabular-nums;line-height:1}
.lvl .vl small{font-size:12px;color:var(--dim);margin-left:2px}
.segs{display:flex;gap:2px;height:20px}
.segs i{flex:1;border-radius:1px;background:var(--off);transition:background .12s}
.sc{display:flex;justify-content:space-between;font-size:10px;color:var(--dim);margin-top:3px}

.gauges{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.gauge{background:var(--card2);border:1px solid var(--line);border-radius:8px;padding:10px 8px 6px;text-align:center}
.gauge svg{width:100%;max-width:190px;display:block;margin:0 auto}
.gauge .gl{font-size:10px;text-transform:uppercase;letter-spacing:.8px;color:var(--dim)}
.gauge .gs{font-size:13px;margin-top:2px;min-height:1.3em}
.gv{font:600 21px/1 ui-monospace,Menlo,monospace}
.gu{font:400 9px/1 sans-serif;fill:var(--dim)}
.gt{font:400 7px/1 sans-serif;fill:var(--dim)}
.gz{stroke-opacity:.22}
:root[data-theme="light"] .gz{stroke-opacity:.4}
@media (prefers-color-scheme: light){:root:not([data-theme="dark"]) .gz{stroke-opacity:.4}}

.tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(104px,1fr));gap:10px}
.t{background:var(--card2);border:1px solid var(--line);border-radius:8px;padding:10px}
.t .k{font-size:10px;text-transform:uppercase;letter-spacing:.8px;color:var(--dim)}
.t .v{font-size:20px;font-variant-numeric:tabular-nums;margin-top:3px;line-height:1.2}
.t .v small{font-size:11px;color:var(--dim)}
.t .sub{font-size:10px;color:var(--dim);margin-top:3px;line-height:1.3}
.al{display:grid;grid-template-columns:repeat(auto-fit,minmax(178px,1fr));gap:8px}
.al div{display:flex;align-items:center;gap:8px;font-size:13px;color:var(--dim)}
.al div.hit{color:var(--bad);font-weight:600}
.offline{position:fixed;left:0;right:0;top:0;background:var(--bad);color:#fff;
text-align:center;padding:8px;font-size:13px;font-weight:600;z-index:20;display:none}
.offline.on{display:block}
.alarmbar{position:fixed;left:0;right:0;top:0;background:var(--bad);color:#fff;
display:none;align-items:center;gap:14px;padding:8px 14px;font-size:14px;
font-weight:600;z-index:21}
.alarmbar.on{display:flex}
.alarmbar .hbtn{margin-left:auto;height:30px;background:rgba(0,0,0,.25);
border-color:rgba(255,255,255,.35);color:#fff;font-weight:600}
@keyframes abpulse{0%,100%{opacity:1}50%{opacity:.55}}
.alarmbar.on{animation:abpulse 1.1s ease-in-out infinite}
body.off .wrap{opacity:.4;filter:grayscale(.6)}
/* The banners are pinned to the top - otherwise they cover the header */
body.off .wrap,body.alarm .wrap{padding-top:52px}
.note{font-size:12px;color:var(--dim);margin-top:10px;min-height:1em}
.note.warn{color:var(--warn)}
.sw{display:flex;align-items:center;gap:9px;cursor:pointer;user-select:none}
.sw i{width:42px;height:23px;border-radius:12px;background:var(--sw);position:relative;transition:.15s;flex:0 0 auto}
.sw i::after{content:"";position:absolute;top:3px;left:3px;width:17px;height:17px;border-radius:50%;
background:#fff;transition:.15s}.sw.on i{background:var(--ok)}.sw.on i::after{left:22px}

.scrim{position:fixed;inset:0;background:rgba(0,0,0,.6);opacity:0;pointer-events:none;transition:.18s;z-index:9}
.scrim.on{opacity:1;pointer-events:auto}
.panel{position:fixed;top:0;right:0;bottom:0;width:min(440px,100%);background:var(--bg);
border-left:1px solid var(--line);padding:18px;overflow-y:auto;z-index:10;
transform:translateX(100%);transition:transform .18s}
.panel.on{transform:none}
.panel h2{font-size:15px;margin:0;font-weight:600;color:var(--fg);text-transform:none;letter-spacing:0}
.ph{display:flex;align-items:center;margin-bottom:16px}
.fld{margin-bottom:12px}
label{font-size:12px;color:var(--dim);display:block;margin-bottom:4px}
input{background:var(--card2);color:var(--fg);border:1px solid var(--line);border-radius:6px;
padding:9px;font:inherit;width:100%}
input[type=file]{padding:7px}
.sec{border-top:1px solid var(--line);margin-top:18px;padding-top:16px}
.hint{font-size:11px;color:var(--dim);margin-top:8px;line-height:1.5}
</style></head><body><div class="offline" id="off"></div>
<div class="alarmbar" id="ab2"><span id="abTxt"></span><button class="hbtn" id="bMute"></button></div>
<div class="wrap">

<header><h1>JUMA PA-100D</h1>
<span class="pill"><span class="dot" id="dPa"></span>PA</span>
<span class="pill"><span class="dot" id="dTci"></span>TCI <span id="tciQrg">-</span></span>
<code id="raw"></code>
<button class="hbtn" id="bGear" style="margin-left:auto" title="Setup">&#9881;</button></header>

<div class="cols">
<section>
  <div class="card"><div class="row">
    <span class="state" id="state">--</span>
    <span class="txb" id="txb">RX</span>
    <span style="margin-left:auto"></span>
    <button id="bOp">OPERATE</button><button id="bSb">STANDBY</button>
  </div><div class="note" id="note"></div></div>

  <div class="card"><h2><span data-t="bandHdr"></span> <span id="bandNow" style="color:var(--fg)">-</span></h2>
    <div class="bands" id="bands"></div>
    <div class="row" style="margin-top:14px">
      <span class="sellb" data-t="tSel"></span>
      <span class="seg" style="margin-left:auto">
        <button class="hbtn" id="bSelM" data-t="manual"></button><button
                class="hbtn" id="bSelA" data-t="auto"></button></span></div>
    <div class="hint" id="aselSub"></div>
    <div class="row" style="margin-top:14px">
      <span class="sw" id="ab"><i></i><span data-t="abLabel"></span></span></div>
  </div>

  <div class="card"><h2 data-t="attHdr"></h2><div class="row" id="gains"></div>
    <div id="gnow" style="margin-top:10px;font-size:17px">-</div>
  </div>
</section>

<section>
  <div class="card"><h2 data-t="lvlHdr"></h2>
    <div class="lvl" id="lRf"></div>
    <div class="lvl" id="lSwr"></div>
  </div>

  <div class="card"><h2 data-t="thHdr"></h2><div class="gauges">
    <div class="gauge"><div class="gl" data-t="tTemp"></div><div id="gTmp"></div>
      <div class="gs" id="gTmpS">&nbsp;</div></div>
    <div class="gauge"><div class="gl" data-t="tFan"></div><div id="gFan"></div>
      <div class="gs" id="gFanS">&nbsp;</div></div>
    <div class="gauge"><div class="gl" data-t="tVolt"></div><div id="gVolt"></div>
      <div class="gs" id="gVoltS">&nbsp;</div></div>
    <div class="gauge"><div class="gl" data-t="tAmp"></div><div id="gAmp"></div>
      <div class="gs" id="gAmpS">&nbsp;</div></div>
  </div></div>

  <div class="card"><h2 data-t="alHdr"></h2><div class="al" id="alarms"></div>
    <div class="row" style="margin-top:14px"><button id="bClr" data-t="bClear"></button></div>
  </div>
</section>
</div></div>

<div class="scrim" id="scrim"></div>
<div class="panel" id="panel">
  <div class="ph"><h2 data-t="cfgHdr"></h2>
    <button class="hbtn" id="bClose" style="margin-left:auto;font-size:17px;padding:0">&times;</button></div>

  <form id="cfg">
    <div class="fld"><label data-t="lHost"></label>
      <input name="hostname" id="hostname" placeholder="juma-pa"></div>
    <div class="fld"><label data-t="lSsid"></label><input name="ssid" id="ssid"></div>
    <div class="fld"><label data-t="lPass"></label>
      <input name="pass" type="password" data-tp="phPass"></div>
    <div class="fld"><label data-t="lTciHost"></label>
      <input name="tcihost" id="tcihost" placeholder="192.168.1.20"></div>
    <div class="fld"><label data-t="lTciPort"></label><input name="tciport" id="tciport" value="50002"></div>
    <div class="row" style="margin:14px 0">
      <span class="sw" id="tcien"><i></i><span data-t="lTciOn"></span></span></div>
    <div class="row" style="margin:14px 0">
      <span class="sw" id="tcilosta"><i></i><span data-t="lTciLost"></span></span></div>
    <div class="hint" data-t="tciLostHint" style="margin-bottom:14px"></div>
    <button type="submit" style="width:100%" data-t="bSave"></button>
    <div class="hint" id="cfgSt"></div>
  </form>

  <div class="sec"><label data-t="lFw"></label>
    <div class="fld"><input type="file" id="fw" accept=".bin"></div>
    <button id="bFw" style="width:100%" data-t="bUpload"></button>
    <div class="hint" id="fwSt"></div>
    <div class="row" style="margin-top:12px">
      <span class="sw" id="otastby"><i></i><span data-t="lOtaStby"></span></span></div>
    <div class="hint" data-t="otaStbyHint"></div>
  </div>

  <div class="sec"><label data-t="lOff"></label>
    <button class="danger" id="bOff" style="width:100%">Power OFF</button>
    <div class="hint" data-th="offHint"></div>
  </div>

  <div class="sec"><label data-t="tTemp"></label>
    <div class="row" style="margin:10px 0">
      <span class="sw" id="tempalarm"><i></i><span data-t="lTempAlarm"></span></span></div>
    <div class="row">
      <div style="flex:1"><label data-t="lTempWarn"></label>
        <input name="tempwarn" id="tempwarn" inputmode="numeric"></div>
      <div style="flex:1"><label data-t="lTempHigh"></label>
        <input name="temphigh" id="temphigh" inputmode="numeric"></div></div>
    <div class="hint" data-t="tempHint"></div>
    <div style="border-top:1px solid var(--line);margin-top:14px;padding-top:14px"></div>
    <div class="row" style="margin:0 0 10px">
      <span class="sw" id="swralarm"><i></i><span data-t="lSwrAlarm"></span></span></div>
    <div class="row">
      <div style="flex:1"><label data-t="lSwrWarn"></label>
        <input id="swrwarn" inputmode="decimal"></div>
      <div style="flex:1"><label data-t="lSwrHigh"></label>
        <input id="swrhigh" inputmode="decimal"></div></div>
    <div class="hint" data-t="swrHint"></div>
  </div>

  <div class="sec"><label data-t="alHdr"></label>
    <div class="row" style="margin:10px 0">
      <span class="sw" id="snd"><i></i><span data-t="lSound"></span></span></div>
    <button class="hbtn" id="bNotify" style="width:100%;height:38px" data-t="bNotify"></button>
    <div class="hint" id="notifySt"></div>
    <div class="hint" data-t="soundHint"></div>
  </div>

  <div class="sec"><label data-t="lTheme"></label>
    <div class="row"><span class="seg" style="width:100%">
      <button class="hbtn" id="thSys" style="flex:1" data-t="thSystem"></button><button
              class="hbtn" id="thLight" style="flex:1" data-t="thLight"></button><button
              class="hbtn" id="thDark" style="flex:1" data-t="thDark"></button></span></div>
  </div>

  <div class="sec"><label data-t="lLang"></label>
    <div class="row"><button class="hbtn" id="bDe" style="flex:1">Deutsch</button>
      <button class="hbtn" id="bEn" style="flex:1">English</button></div>
  </div>
</div>

<script>
// --- Wording --------------------------------------------------------------
const L={
de:{bandHdr:"Band — PA meldet",abLabel:"Bandwahl per TCI",
attHdr:"Abschwächer",
lvlHdr:"Pegel",thHdr:"Temperatur, Lüfter, Versorgung",alHdr:"Alarme",
tTemp:"PA Temp",tFan:"Lüfter",tVolt:"Spannung",tAmp:"Strom",tSel:"Bandwahl der PA",
tAtt:"Abschwächer",bClear:"Alarm quittieren",alarmTitle:"JUMA PA-100D: Alarm",alarmBody:"Die Endstufe meldet: %s",bMute:"Stummschalten",bMuted:"Stumm",lSound:"Akustischer Alarm",bNotify:"Benachrichtigungen erlauben",notifyOn:"Benachrichtigungen aktiv",notifyNo:"Benachrichtigungen erlaubt der Browser nur über HTTPS. Diese Seite läuft über http://, deshalb geht es hier nicht. Der Alarmton und das Banner funktionieren unabhängig davon.",notifyDenied:"Benachrichtigungen wurden abgelehnt",soundHint:"Die PA piepst nur vor Ort. Der Browser wiederholt den Alarmton alle 5 s, bis er quittiert ist oder der Alarm weg ist. Der Ton startet erst, nachdem die Seite einmal angeklickt wurde — so will es der Browser.",vNorm:"normal",vPre:"Vorwarnung",vUnder:"Unterspannung",vHigh:"erhöht",vOver:"Überspannung",iTrip:"Trip bei %s A",
cfgHdr:"Konfiguration",lHost:"Gerätename (mDNS, OTA)",lSsid:"WLAN SSID",lPass:"WLAN Passwort",phPass:"unverändert lassen",
lTciHost:"TCI Host (SDR-Software)",lTciPort:"TCI Port",lTciOn:"TCI aktiv",lTciLost:"Bei TCI-Verlust auf Automatik der PA",tciLostHint:"Schickt nach 15 s ohne TCI ein =A. Welche Methode die PA dann nutzt, steht in ihrer eigenen Konfiguration (F-Sense, FT-817, Yaesu CAT, KX2/KX3, JUMA-TRX2) — steht sie dort auf Manual, bringt =A nichts. Ohne diesen Schalter bleibt die PA auf dem zuletzt kommandierten Band, weil =Bn sie von A auf M schaltet.",
bSave:"Speichern & neu starten",lFw:"Firmware-Update",bUpload:"Hochladen",
lOff:"Verstärker abschalten",lLang:"Sprache",lTheme:"Darstellung",thSystem:"System",thLight:"Hell",thDark:"Dunkel",lOtaStby:"Vor dem Update auf STANDBY",otaStbyHint:"Wirkt sofort. Schickt =S, bevor die neue Firmware geschrieben wird. Während des Schreibens und des Neustarts regelt nichts die PA. Aus lassen, wenn dir Entwicklungs-Uploads nicht die Betriebsart wegnehmen sollen.",
offHint:"Schickt <code>=P0</code> ohne Zustandsspeicherung. Einschalten geht nur am Gerät — zweimal drücken zur Bestätigung.",
fans:["Aus","Langsam","Mittel","Schnell"],
alarms:["SWR zu hoch","Überstrom","Übertemperatur","Überspannung",
        "Unterspannung Vorwarnung","Unterspannung Abschaltung"],
auto:"Automatik",manual:"Manuell",selConflict:"PA wählt selbst, obwohl TCI-Bandwahl an ist",selLocked:"Bei aktiver TCI-Bandwahl bestimmt der ESP32 das Band und hält die PA auf Manuell.",cels:"Celsius",fahr:"Fahrenheit",
tNorm:"normal",tWarm:"warm",tHot:"zu heiß",tooHot:"Temperatur %s°",badSwr:"SWR %s",lSwrAlarm:"SWR-Vorwarnung",lSwrWarn:"Warnung ab",lSwrHigh:"Rot ab",swrHint:"Wirkt sofort. Die Abschaltgrenze der PA ist werksseitig 3,0 (einstellbar 1,0–10,0) und steht nicht in der Statusmeldung — hier den eigenen Wert eintragen und darunter warnen lassen. Im Empfang meldet die PA 0,0, die Warnung greift also nur beim Senden.",lTempWarn:"Warnung ab (°)",lTempHigh:"Rot ab (°)",lTempAlarm:"Temperatur-Vorwarnung",tempHint:"Wirkt sofort, ohne Speichern. Die PA setzt ihr Alarmbit erst beim Abschalten — dann bleibt keine Zeit mehr. Diese Warnung schlägt vorher an, mit demselben Ton und Banner. Sinnvoll ist ein Wert deutlich unter der Abschaltgrenze des Geräts (Werksvorgabe 70°, einstellbar 50–100°).",unitStep:"Stufe",
running:"Läuft: ",noFile:"keine Datei gewählt",auth:"Anmeldung…",loading:"lade %s kB…",
upOk:"OK — Gerät startet neu",upErr:"Fehler %s",upAbort:"Übertragung abgebrochen",
saving:"speichere…",restarting:"Gerät startet neu",saved:"Gespeichert — Gerät startet neu",wsLost:"Verbindung zum Gerät unterbrochen — versuche erneut…",
confirm:"Wirklich? Nochmal drücken",
nAboff:"TCI-Bandwahl aus — die PA folgt der SDR-Software nicht",
nTcioff:"TCI-Client ist abgeschaltet",
nTcidis:"TCI nicht verbunden",nTciauto:"TCI weg — PA auf eigene Bandwahl (=A) zurückgestellt",nSelstuck:"PA bleibt auf eigener Bandwahl, obwohl die TCI-Bandwahl sie auf Manuell holen will",
nTcinofreq:"TCI verbunden, aber noch keine QRG empfangen",

nPaoff:"PA antwortet nicht",
nUnsupported:"QRG %s — die PA-100D deckt das Band nicht ab, Band bleibt unverändert",
nBandok:"Band folgt TCI: %s",
nBandset:"Band umgeschaltet: %s"},

en:{bandHdr:"Band — PA reports",abLabel:"Band select via TCI",
attHdr:"Attenuator",
lvlHdr:"Levels",thHdr:"Temperature, fan, supply",alHdr:"Alarms",
tTemp:"PA temp",tFan:"Fan",tVolt:"Voltage",tAmp:"Current",tSel:"PA band select",
tAtt:"Attenuator",bClear:"Clear alarm",alarmTitle:"JUMA PA-100D: alarm",alarmBody:"The amplifier reports: %s",bMute:"Mute",bMuted:"Muted",lSound:"Audible alarm",bNotify:"Enable notifications",notifyOn:"Notifications active",notifyNo:"Browsers only allow notifications over HTTPS. This page runs over http://, so it cannot work here. The alarm tone and banner work regardless.",notifyDenied:"Notifications were denied",soundHint:"The PA only beeps locally. The browser repeats the alarm tone every 5 s until acknowledged or the alarm clears. Sound starts only after the page has been clicked once — browser policy.",vNorm:"normal",vPre:"pre-limit",vUnder:"under-voltage",vHigh:"elevated",vOver:"over-voltage",iTrip:"trip at %s A",
cfgHdr:"Setup",lHost:"Device name (mDNS, OTA)",lSsid:"Wi-Fi SSID",lPass:"Wi-Fi password",phPass:"leave unchanged",
lTciHost:"TCI host (SDR software)",lTciPort:"TCI port",lTciOn:"TCI enabled",lTciLost:"Fall back to the PA\u2019s own band select",tciLostHint:"Sends =A after 15 s without TCI. Which method the PA then uses is set in its own configuration (F-Sense, FT-817, Yaesu CAT, KX2/KX3, JUMA-TRX2) — if that is set to Manual, =A achieves nothing. Without this switch the PA stays on the last commanded band, because =Bn moves it from A to M.",
bSave:"Save & restart",lFw:"Firmware update",bUpload:"Upload",
lOff:"Power down amplifier",lLang:"Language",lTheme:"Appearance",thSystem:"System",thLight:"Light",thDark:"Dark",lOtaStby:"Standby before update",otaStbyHint:"Applies immediately. Sends =S before the new firmware is written. Nothing controls the PA while writing and rebooting. Turn off if development uploads should not take away the operating state.",
offHint:"Sends <code>=P0</code> without saving state. Powering on is only possible at the unit — press twice to confirm.",
fans:["Off","Slow","Medium","Fast"],
alarms:["High SWR","Over-current","High temperature","High voltage",
        "Low voltage pre-limit","Low voltage final limit"],
auto:"Automatic",manual:"Manual",selConflict:"PA selects on its own while TCI band select is on",selLocked:"With TCI band select on, the ESP32 determines the band and holds the PA on Manual.",cels:"Celsius",fahr:"Fahrenheit",
tNorm:"normal",tWarm:"warm",tHot:"too hot",tooHot:"temperature %s°",badSwr:"SWR %s",lSwrAlarm:"SWR pre-warning",lSwrWarn:"Warn above",lSwrHigh:"Red above",swrHint:"Applies immediately. The unit\u2019s trip limit is 3.0 by default (adjustable 1.0–10.0) and is not part of the status message — enter your own value and warn below it. While receiving the PA reports 0.0, so the warning only applies during transmit.",lTempWarn:"Warn above (°)",lTempHigh:"Red above (°)",lTempAlarm:"Temperature pre-warning",tempHint:"Applies immediately, no saving needed. The PA only sets its alarm bit when it shuts down — too late to react. This warning trips earlier, with the same tone and banner. Pick a value well below the unit\u2019s cut-out (factory default 70°, adjustable 50–100°).",unitStep:"Step",
running:"Running: ",noFile:"no file selected",auth:"Authenticating…",loading:"uploading %s kB…",
upOk:"OK — device restarting",upErr:"Error %s",upAbort:"transfer aborted",
saving:"saving…",restarting:"device restarting",saved:"Saved — device restarting",wsLost:"Connection to the device lost — retrying…",
confirm:"Confirm? Press again",
nAboff:"TCI band select off — the PA does not follow the SDR software",
nTcioff:"TCI client is disabled",
nTcidis:"TCI not connected",nTciauto:"TCI lost — PA switched back to its own band select (=A)",nSelstuck:"PA stays on its own band select although TCI band select wants it on Manual",
nTcinofreq:"TCI connected, but no frequency received yet",

nPaoff:"PA not responding",
nUnsupported:"QRG %s — the PA-100D does not cover this band, band left unchanged",
nBandok:"Band follows TCI: %s",
nBandset:"Band switched: %s"}};

let lang = localStorage.getItem("lang") ||
  ((navigator.language||"").toLowerCase().indexOf("de")===0 ? "de" : "en");
function t(k,a){const v=L[lang][k];return (a===undefined)?v:String(v).replace("%s",a)}

const BANDS=[[1,"160m"],[2,"80m"],[3,"40m"],[4,"30m"],[5,"20m"],[6,"17m"],[7,"15m"],[8,"12m"],[9,"10m"]];
const AMASK=[1,2,4,8,16,32];
// Read from the CSS so switching light/dark also reaches the gauges
let C={};
function readColors(){
  const cs=getComputedStyle(document.documentElement);
  ["ok","warn","bad","off","sw","fg"].forEach(function(k){
    C[k]=cs.getPropertyValue("--"+k).trim()});
}
readColors();
const $=i=>document.getElementById(i);
let ws,st={},offArm=0;

// --- Level bars -----------------------------------------------------------
const NSEG=36;
function mkLevel(el,o){
  el.dataset.cfg=JSON.stringify(o);
  let s='<div class="hd"><span class="lb">'+o.label+'</span>'+
        '<span class="vl"><span class="n">-</span><small>'+(o.unit||'')+'</small></span></div>'+
        '<div class="segs">';
  for(let i=0;i<NSEG;i++)s+='<i></i>';
  s+='</div><div class="sc"><span>'+o.min+'</span><span>'+o.warn+'</span>'+
     '<span>'+o.high+'</span><span>'+o.max+'</span></div>';
  el.innerHTML=s;
}
// Colour by the segment's own position - that way the bar shows the zones
// instead of flipping over entirely once a threshold is passed.
function segColor(o,i){
  const val=o.min+(i+0.5)*(o.max-o.min)/NSEG;
  return val>=o.high?C.bad:val>=o.warn?C.warn:C.ok;
}
function setLevel(el,val,dec){
  const o=JSON.parse(el.dataset.cfg);
  const n=el.querySelector(".n"),segs=el.querySelectorAll(".segs i");
  const ok=val!==null&&val!==undefined&&!isNaN(val);
  n.textContent=ok?Number(val).toFixed(dec===undefined?1:dec):"-";
  const lit=ok?Math.round(Math.max(0,Math.min(1,(val-o.min)/(o.max-o.min)))*NSEG):0;
  segs.forEach((s,i)=>{s.style.background=i<lit?segColor(o,i):C.off});
  n.style.color=!ok?"var(--dim)":val>=o.high?C.bad:val>=o.warn?C.warn:"var(--fg)";
}
// The SWR zones come from the settings - the PA's trip limit (factory default
// 3.0) is not part of the status message.
let SWRWARN=2.0,SWRHIGH=2.5;
function buildLevels(){
  mkLevel($("lRf"), {label:"RF",   min:0, max:150, warn:100, high:120, unit:" W"});
  mkLevel($("lSwr"),{label:"VSWR", min:1, max:3, warn:SWRWARN, high:SWRHIGH, unit:""});
}
buildLevels();

// --- Dial gauges: 180 degree arc with zone colours ------------------------
const GA1=180,GR=40,GCX=50,GCY=47;
function pol(r,deg){const a=(deg-180)*Math.PI/180;return[GCX+r*Math.cos(a),GCY+r*Math.sin(a)]}
function arcPath(r,d0,d1){
  const p0=pol(r,d0),p1=pol(r,d1);
  return "M"+p0[0].toFixed(2)+" "+p0[1].toFixed(2)+" A"+r+" "+r+" 0 "+
         ((d1-d0)>180?1:0)+" 1 "+p1[0].toFixed(2)+" "+p1[1].toFixed(2);
}
// mark=true: a marker at the value instead of an arc filled from the bottom.
// For quantities that live in a narrow band - a filled arc would cover the
// zones there and, once exceeded, colour the whole range as if everything
// were critical.
function mkGauge(el,zones,unit,ticks,mark){
  el.dataset.mark = mark ? "1" : "";
  let g='<svg viewBox="-10 -10 120 74">';
  zones.forEach((z,i)=>{
    const f0=i?zones[i-1][0]:0;
    // opacity via CSS so the light theme can raise it
    g+='<path class="gz" d="'+arcPath(GR,f0*GA1,z[0]*GA1)+
       '" fill="none" stroke="'+z[1]+'" stroke-width="11"/>';
  });
  g+='<path class="gval" d="" fill="none" stroke="'+C.off+'" stroke-width="11"/>';
  (ticks||[]).forEach(function(tk){
    const p=pol(GR+11,tk[0]*GA1);
    g+='<text class="gt" x="'+p[0].toFixed(1)+'" y="'+p[1].toFixed(1)+
       '" text-anchor="middle" dominant-baseline="middle">'+tk[1]+'</text>';
  });
  g+='<text class="gv" x="50" y="44" text-anchor="middle" fill='+C.off+'>-</text>'+
     '<text class="gu" x="50" y="55" text-anchor="middle">'+unit+'</text></svg>';
  el.innerHTML=g;
}
function setGauge(el,frac,text,color){
  const mark=el.dataset.mark==="1";
  const sv=el.querySelector("svg"); if(!sv)return;
  const val=sv.querySelector(".gval"),num=sv.querySelector(".gv");
  const f=Math.max(0,Math.min(1,frac||0));
  if(mark){
    const a=f*GA1, w=2.6;
    val.setAttribute("d", arcPath(GR,Math.max(0,a-w),Math.min(GA1,a+w)));
    val.setAttribute("stroke",C.fg);            // Kontrast statt Zonenfarbe,
    val.setAttribute("stroke-width","15");      // otherwise it disappears into the zone
    num.textContent=text; num.setAttribute("fill",color);
    return;
  }else{
    val.setAttribute("d", f<=0.001 ? "" : arcPath(GR,0,f*GA1));
  }
  val.setAttribute("stroke",color);
  num.textContent=text;
  num.setAttribute("fill",color);
  // "13.68" is twice as wide as "27" - otherwise the number hits the arc
  num.setAttribute("font-size", text.length>=5 ? 15 : text.length>=4 ? 18 : 21);
}
// Scale ends fixed, warn and red thresholds come from the settings - the PA's
// cut-out limit is not part of the status message, so it has to be maintained
// by hand.
const TMIN=20,TMAX=80;
let TWARN=50,THIGH=60;
// The PA can report in Fahrenheit - unit and scale labels then have to follow,
// while the zones stay the same temperatures.
let tempF=false;
let FW=(TWARN-TMIN)/(TMAX-TMIN), FH=(THIGH-TMIN)/(TMAX-TMIN);

// Voltage limits are defaults and adjustment ranges from the manual:
// under-voltage 11.00 V, pre-limit 11.20 V, over-voltage adjustable from
// 14.00 V with a default of 14.80 V, nominal 13.80 V. If the unit is set
// differently, adjust here.
const VMIN=10.0,VMAX=15.5,VUV=11.0,VPRE=11.2,VOVA=14.0,VOV=14.8;
const vf=function(x){return (x-VMIN)/(VMAX-VMIN)};

// Per the manual the 24 A trip (MAX4373) is also full scale on the unit's own
// meter. The warning zones at 80 % and 90 % of it are derived - the manual
// names only the trip itself.
const ITRIP=24,IW1=.8,IW2=.9;

function buildGauges(){
  FW=(TWARN-TMIN)/(TMAX-TMIN); FH=(THIGH-TMIN)/(TMAX-TMIN);
  const lab=function(c){return String(tempF?Math.round(c*9/5+32):c)};
  mkGauge($("gTmp"),[[FW,C.ok],[FH,C.warn],[1,C.bad]],
          tempF?"°F":"°C",
          [[0,lab(TMIN)],[FW,lab(TWARN)],[FH,lab(THIGH)],[1,lab(TMAX)]]);
  mkGauge($("gFan"),[[.25,C.off],[.5,C.ok],[.75,C.warn],[1,C.bad]],
          t("unitStep"),[[.125,"0"],[.375,"1"],[.625,"2"],[.875,"3"]]);
  mkGauge($("gVolt"),[[vf(VUV),C.bad],[vf(VPRE),C.warn],[vf(VOVA),C.ok],
                      [vf(VOV),C.warn],[1,C.bad]],
          "V",[[0,"10"],[vf(VUV),"11"],[vf(13.8),"13.8"],[1,"15.5"]],true);
  $("gVolt").querySelectorAll(".gz").forEach(function(z){z.style.strokeOpacity=".75"});
  mkGauge($("gAmp"),[[IW1,C.ok],[IW2,C.warn],[1,C.bad]],
          "A",[[0,"0"],[.5,"12"],[1,"24"]]);
}

// --- Controls -------------------------------------------------------------
BANDS.forEach(function(b){const e=document.createElement("button");e.textContent=b[1];
e.dataset.b=b[0];e.onclick=function(){send("band",b[0])};$("bands").appendChild(e)});
for(let g=1;g<=4;g++){const e=document.createElement("button");e.textContent="G"+g;
e.dataset.g=g;e.onclick=function(){send("gain",g)};$("gains").appendChild(e)}
AMASK.forEach(function(m,i){const d=document.createElement("div");
d.innerHTML='<span class="dot"></span><span class="an"></span>';
d.dataset.m=m;d.dataset.i=i;$("alarms").appendChild(d)});

$("bOp").onclick=function(){send("operate",1)};
$("bSb").onclick=function(){send("operate",0)};
$("bClr").onclick=function(){send("clear",0)};
$("ab").onclick=function(){send("autoband",st.autoband?0:1)};
// "Auto" = =A. "Manual" has no command of its own - for that the firmware
// sends the band the PA is already on.
$("bSelM").onclick=function(){send("bandsel",0)};
$("bSelA").onclick=function(){send("bandsel",1)};
$("tcien").onclick=function(e){e.currentTarget.classList.toggle("on")};
// Effective at once instead of via the form: these fields sit outside it and
// were not submitted when saving at all.
function liveSwitch(id,cmd){
  $(id).onclick=function(e){
    const on=!e.currentTarget.classList.contains("on");
    e.currentTarget.classList.toggle("on",on);
    send(cmd,on?1:0);
  };
}
liveSwitch("otastby","otastby");
liveSwitch("tempalarm","tempalarm");
liveSwitch("swralarm","swralarm");
// send as tenths so the command stays integral
["swrwarn","swrhigh"].forEach(function(id){
  $(id).onchange=function(){
    const v=Math.round(parseFloat(this.value.replace(",","."))*10);
    if(v>=10&&v<=100)send(id,v);
    else this.value=(st[id==="swrwarn"?"swrWarn":"swrHigh"]||0).toFixed(1);
  };
});
["tempwarn","temphigh"].forEach(function(id){
  $(id).onchange=function(){
    const v=parseInt(this.value,10);
    if(v>=20&&v<=120)send(id,v); else this.value=st[id==="tempwarn"?"tempWarn":"tempHigh"];
  };
});
$("tcilosta").onclick=function(e){e.currentTarget.classList.toggle("on")};

function panel(on){$("panel").classList.toggle("on",on);$("scrim").classList.toggle("on",on);
if(on)applyLang()}
$("bGear").onclick=function(){panel(true)};
$("bClose").onclick=function(){panel(false)};
$("scrim").onclick=function(){panel(false)};
addEventListener("keydown",function(e){if(e.key==="Escape")panel(false)});

$("bOff").onclick=function(e){const b=e.currentTarget;
if(Date.now()-offArm>3000){offArm=Date.now();b.textContent=t("confirm");
setTimeout(function(){if(Date.now()-offArm>=3000)b.textContent="Power OFF"},3100);return}
offArm=0;b.textContent="Power OFF";send("poweroff",0)};

$("cfg").onsubmit=function(e){e.preventDefault();const f=new FormData(e.target);
f.set("tcien",$("tcien").classList.contains("on")?"1":"0");
f.set("tcilosta",$("tcilosta").classList.contains("on")?"1":"0");
$("cfgSt").textContent=t("saving");
fetch("/api/config",{method:"POST",body:new URLSearchParams(f)})
.then(function(){$("cfgSt").textContent=t("saved")})
.catch(function(){$("cfgSt").textContent=t("restarting")})};

$("bFw").onclick=function(){const f=$("fw").files[0];
if(!f){$("fwSt").textContent=t("noFile");return}
const fd=new FormData();fd.append("firmware",f,f.name);
// The protected GET first - then the browser asks for credentials before the
// file is sent, rather than again afterwards.
$("fwSt").textContent=t("auth");
fetch("/update",{method:"GET",credentials:"include"}).then(function(r){
if(!r.ok){$("fwSt").textContent=t("upErr",r.status);return}
$("fwSt").textContent=t("loading",f.size/1024|0);
const x=new XMLHttpRequest();x.open("POST","/update",true);x.withCredentials=true;
x.upload.onprogress=function(e){if(e.lengthComputable)
$("fwSt").textContent=Math.round(e.loaded/e.total*100)+" %"};
x.onload=function(){$("fwSt").textContent=x.status==200?t("upOk"):t("upErr",x.status)};
x.onerror=function(){$("fwSt").textContent=t("upAbort")};x.send(fd);
}).catch(function(){$("fwSt").textContent=t("upAbort")})};

// --- Raising alarms -------------------------------------------------------
// The PA only beeps locally. The Notification API is moreover restricted to a
// "secure context" (HTTPS or localhost) - over http:// to a LAN address the
// browser does not offer it at all. So the tone inside the page is the base,
// and notifications come on top only where the browser allows them.
let ac=null,beepT=null,titleT=null,muted=false,lastAl=0,lastHot=0,lastBad=0,tCelCur=0;
const origTitle=document.title;
let sound=localStorage.getItem("sound")!=="0";

function audio(){
  if(!ac){try{ac=new (window.AudioContext||window.webkitAudioContext)()}catch(e){return null}}
  if(ac.state==="suspended")ac.resume();
  return ac;
}
// Two short tones, like the PA's own beeper
function beep(){
  if(!sound||muted)return;
  const c=audio(); if(!c)return;
  [0,.35].forEach(function(dt){
    const o=c.createOscillator(),g=c.createGain();
    o.type="square"; o.frequency.value=1180;
    o.connect(g); g.connect(c.destination);
    const t0=c.currentTime+dt;
    g.gain.setValueAtTime(.0001,t0);
    g.gain.exponentialRampToValueAtTime(.25,t0+.01);
    g.gain.exponentialRampToValueAtTime(.0001,t0+.22);
    o.start(t0); o.stop(t0+.24);
  });
}
function alarmNames(mask){
  return AMASK.map(function(m,i){return (mask&m)?t("alarms")[i]:null})
              .filter(Boolean).join(", ");
}
function notify(txt){
  if(!("Notification" in window)||Notification.permission!=="granted")return;
  try{new Notification(t("alarmTitle"),
      {body:t("alarmBody",txt),tag:"juma-alarm",renotify:true})}catch(e){}
}
function alarmOn(mask,hot,swr){
  $("ab2").className="alarmbar on";
  document.body.classList.add("alarm");
  let txt=alarmNames(mask);
  if(hot)txt=(txt?txt+", ":"")+t("tooHot",hot);
  if(swr)txt=(txt?txt+", ":"")+t("badSwr",swr.toFixed(1));
  $("abTxt").textContent=t("alarmBody",txt);
  if(!titleT)titleT=setInterval(function(){
    document.title=document.title===origTitle?"\u26A0 "+t("alarmTitle"):origTitle},1200);
  if(!beepT&&!muted){beep();beepT=setInterval(beep,5000)}
}
function alarmOff(){
  $("ab2").className="alarmbar";
  document.body.classList.remove("alarm");
  muted=false; $("bMute").textContent=t("bMute");
  if(beepT){clearInterval(beepT);beepT=null}
  if(titleT){clearInterval(titleT);titleT=null;document.title=origTitle}
}
$("bMute").onclick=function(){
  muted=true; $("bMute").textContent=t("bMuted");
  if(beepT){clearInterval(beepT);beepT=null}
};
$("snd").onclick=function(e){
  sound=!e.currentTarget.classList.contains("on");
  e.currentTarget.classList.toggle("on",sound);
  localStorage.setItem("sound",sound?"1":"0");
  if(sound)beep();
};
function notifyStatus(){
  // Chrome keeps the Notification object on http:// as well and silently sets
  // the permission to "denied". That looks as if the user had refused - so ask
  // for the real reason and disable the button.
  const insecure=!window.isSecureContext;
  $("bNotify").disabled = insecure || !("Notification" in window);
  if(!("Notification" in window)||insecure){$("notifySt").textContent=t("notifyNo");return}
  if(Notification.permission==="granted")$("notifySt").textContent=t("notifyOn");
  else if(Notification.permission==="denied")$("notifySt").textContent=t("notifyDenied");
  else $("notifySt").textContent="";
}
$("bNotify").onclick=function(){
  if(!("Notification" in window)){$("notifySt").textContent=t("notifyNo");return}
  try{Notification.requestPermission().then(notifyStatus).catch(notifyStatus)}
  catch(e){$("notifySt").textContent=t("notifyNo")}
};

function send(c,v){if(ws&&ws.readyState==1)ws.send(c+":"+v)}

// --- Language -------------------------------------------------------------
function setLang(l){lang=l;localStorage.setItem("lang",l);applyLang()}

// "" = follow the system
let theme=localStorage.getItem("theme")||"";
function applyTheme(){
  if(theme)document.documentElement.setAttribute("data-theme",theme);
  else     document.documentElement.removeAttribute("data-theme");
  localStorage.setItem("theme",theme);
  readColors();
  ["thSys","thLight","thDark"].forEach(function(i,n){
    $(i).className="hbtn"+((["","light","dark"][n]===theme)?" act":"")});
  buildGauges();
  if(st.raw!==undefined)render(st);
}
$("thSys").onclick=function(){theme="";applyTheme()};
$("thLight").onclick=function(){theme="light";applyTheme()};
$("thDark").onclick=function(){theme="dark";applyTheme()};
if(window.matchMedia)matchMedia("(prefers-color-scheme: light)")
  .addEventListener("change",function(){if(!theme)applyTheme()});
$("bDe").onclick=function(){setLang("de")};
$("bEn").onclick=function(){setLang("en")};

function applyLang(){
  document.documentElement.lang=lang;
  document.querySelectorAll("[data-t]").forEach(function(e){e.textContent=t(e.dataset.t)});
  document.querySelectorAll("[data-th]").forEach(function(e){e.innerHTML=t(e.dataset.th)});
  document.querySelectorAll("[data-tp]").forEach(function(e){e.placeholder=t(e.dataset.tp)});
  document.querySelectorAll("#alarms div").forEach(function(d){
    d.querySelector(".an").textContent=t("alarms")[+d.dataset.i]});
  $("snd").className="sw"+(sound?" on":"");
  $("bMute").textContent=muted?t("bMuted"):t("bMute");
  notifyStatus();
  if($("ab2").className.indexOf("on")>=0&&st.alarms)alarmOn(st.alarms);
  $("bDe").className="hbtn"+(lang==="de"?" act":"");
  $("bEn").className="hbtn"+(lang==="en"?" act":"");
  buildGauges();
  if($("off").className.indexOf("on")>=0)$("off").textContent=t("wsLost");
  if(st.raw!==undefined)render(st);
}

// --- Rendering the state --------------------------------------------------
// Only genuine obstacles are coloured as a warning - "automatic off" or "band
// follows TCI" are state information, not problems.
const NWARN={tcidis:1,tcinofreq:1,unsupported:1,paoff:1,selstuck:1};
function noteText(s){
  if(!s.note)return "";
  const k="n"+s.note.charAt(0).toUpperCase()+s.note.slice(1);
  return L[lang][k] ? t(k,s.noteArg||"") : s.note;
}
function render(s){st=s;
if(s.tempWarn&&(s.tempWarn!==TWARN||s.tempHigh!==THIGH)){
  TWARN=s.tempWarn; THIGH=s.tempHigh; buildGauges();
}
if(s.swrWarn&&(s.swrWarn!==SWRWARN||s.swrHigh!==SWRHIGH)){
  SWRWARN=s.swrWarn; SWRHIGH=s.swrHigh; buildLevels();
}
$("dPa").className="dot"+(s.online?" on":"");
$("dTci").className="dot"+(s.tciConn?" on":"");
$("tciQrg").textContent=s.tciHz?(s.tciHz/1e6).toFixed(4)+" MHz "+s.tciBandName:"-";

$("state").textContent=s.online?(s.operate?"OPERATE":"STANDBY"):"OFFLINE";
$("state").style.color=s.online?(s.operate?C.ok:"var(--dim)"):C.bad;
$("bOp").className=s.operate?"op":"";$("bSb").className=s.operate?"":"act";
$("txb").className="txb"+(s.tx?" on":"");$("txb").textContent=s.tx?"TX":"RX";

setLevel($("lRf"), s.online?s.watts:null,1);
setLevel($("lSwr"),s.online?s.swr:null,1);

if(s.online){
  if(tempF!==!s.celsius){tempF=!s.celsius;buildGauges()}
  const tCel=s.celsius?s.temp:(s.temp-32)*5/9;
  tCelCur=tCel;
  const tc=tCel>=THIGH?C.bad:tCel>=TWARN?C.warn:C.ok;
  setGauge($("gTmp"),(tCel-TMIN)/(TMAX-TMIN),String(s.temp),tc);
  $("gTmpS").textContent=tCel>=THIGH?t("tHot"):tCel>=TWARN?t("tWarm"):t("tNorm");
  $("gTmpS").style.color=tc;
  const fc=[C.off,C.ok,C.warn,C.bad][s.fan]||C.off;
  setGauge($("gFan"),(s.fan+1)/4,String(s.fan),fc);
  $("gFanS").textContent=t("fans")[s.fan]||"?";
  $("gFanS").style.color=s.fan?fc:"var(--dim)";

  const v=s.volts;
  const vc=(v<VUV||v>=VOV)?C.bad:(v<VPRE||v>=VOVA)?C.warn:C.ok;
  setGauge($("gVolt"),vf(v),v.toFixed(2),vc);
  $("gVoltS").textContent=v<VUV?t("vUnder"):v<VPRE?t("vPre"):
                          v>=VOV?t("vOver"):v>=VOVA?t("vHigh"):t("vNorm");
  $("gVoltS").style.color=vc;

  const a=s.amps, ac=a>=ITRIP*IW2?C.bad:a>=ITRIP*IW1?C.warn:C.ok;
  setGauge($("gAmp"),a/ITRIP,a.toFixed(1),ac);
  $("gAmpS").textContent=t("iTrip",ITRIP);
  $("gAmpS").style.color=a>=ITRIP*IW1?ac:"var(--dim)";
}else{
  setGauge($("gTmp"),0,"-",C.off);$("gTmpS").innerHTML="&nbsp;";
  setGauge($("gFan"),0,"-",C.off);$("gFanS").innerHTML="&nbsp;";
  setGauge($("gVolt"),0,"-",C.off);$("gVoltS").innerHTML="&nbsp;";
  setGauge($("gAmp"),0,"-",C.off);$("gAmpS").innerHTML="&nbsp;";
}

// In automatic mode the PA picks the band itself via F-Sense and can thus work
// against our =Bn - a problem only while TCI band select is running.
$("bSelM").className="hbtn"+(s.online&&!s.autoSel?" act":"");
$("bSelA").className="hbtn"+(s.online&&s.autoSel?" act":"");
// With TCI band select active the ESP32 determines the band - a change here
// would be undone by the next =Bn straight away.
const locked=s.autoband||!s.online;
$("bSelM").disabled=locked;$("bSelA").disabled=locked;
const conflict=s.online&&s.autoSel&&s.autoband;
$("aselSub").textContent=conflict?t("selConflict"):(s.autoband?t("selLocked"):"");
$("aselSub").style.color=conflict?C.warn:"var(--dim)";
// Which step is active is shown by the G buttons - only the value here
$("gnow").textContent=s.online&&s.gain?[6,4,2,0][s.gain-1]+" dB":"-";

$("bandNow").textContent=s.bandName;
document.querySelectorAll("#bands button").forEach(function(b){
b.className=(+b.dataset.b===s.band)?"act":""});
document.querySelectorAll("#gains button").forEach(function(b){
b.className=(+b.dataset.g===s.gain)?"act":""});
document.querySelectorAll("#alarms div").forEach(function(d){
const hit=(s.alarms&+d.dataset.m)!==0;
d.className=hit?"hit":"";d.firstChild.style.background=hit?C.bad:C.sw});

// The PA raises its alarm bit only when shutting down - by then it is too
// late. So the dashboard warns at a threshold of its own.
const hot=(s.tempAlarm&&s.online&&tCelCur>=TWARN)?s.temp:0;
// SWR only means anything while transmitting - on receive the PA reports 0.0
const bad=(s.swrAlarm&&s.online&&s.swr>0&&s.swr>=SWRWARN)?s.swr:0;
if(s.alarms||hot||bad){
  alarmOn(s.alarms,hot,bad);
  const neu=s.alarms&~lastAl;
  if(neu)notify(alarmNames(neu));
  else if(hot&&!lastHot)notify(t("tooHot",hot));
  else if(bad&&!lastBad)notify(t("badSwr",bad.toFixed(1)));
}else if(lastAl||lastHot||lastBad){alarmOff()}
lastAl=s.alarms; lastHot=hot; lastBad=bad;

$("ab").className="sw"+(s.autoband?" on":"");
$("raw").textContent=s.raw;
$("note").textContent=noteText(s);
$("note").className="note"+(NWARN[s.note]?" warn":"");
$("fwSt").textContent=t("running")+(s.version||"");
if(!$("panel").classList.contains("on")){
  $("hostname").value=s.hostname||"";
  $("tcihost").value=s.tciHost;$("tciport").value=s.tciPort;$("ssid").value=s.ssid;
  $("tcien").className="sw"+(s.tciEnabled?" on":"");
  $("otastby").className="sw"+(s.otaStandby?" on":"");
  $("tcilosta").className="sw"+(s.tciLostAuto?" on":"");
  $("tempalarm").className="sw"+(s.tempAlarm?" on":"");
  $("tempwarn").value=s.tempWarn;
  $("temphigh").value=s.tempHigh;
  $("swralarm").className="sw"+(s.swrAlarm?" on":"");
  $("swrwarn").value=(s.swrWarn||0).toFixed(1);
  $("swrhigh").value=(s.swrHigh||0).toFixed(1)}}

applyLang();
// A dashboard stuck on old values looks like a hang - so the page says
// clearly when the connection is gone.
function setLink(on){
  $("off").className="offline"+(on?"":" on");
  document.body.classList.toggle("off",!on);
  if(!on)$("off").textContent=t("wsLost");
}
// The server can stop sending without closing the connection - then no
// onclose arrives and the page silently holds the last state: green dot, old
// frequency, everything frozen. From the outside that looks like a hung
// interface. Hence a watchdog of our own: if updates stop arriving, the
// connection counts as dead.
let lastMsg=0,wdT=null;
const WD_MS=4000;                    // acht ausgebliebene Aktualisierungen
function conn(){
  try{ws=new WebSocket("ws://"+location.hostname+":81/")}catch(e){setTimeout(conn,2000);return}
  ws.onopen=function(){lastMsg=Date.now();setLink(true)};
  ws.onmessage=function(e){lastMsg=Date.now();setLink(true);render(JSON.parse(e.data))};
  ws.onclose=function(){$("dPa").className="dot";setLink(false);setTimeout(conn,2000)};
  ws.onerror=function(){setLink(false)};
  if(!wdT)wdT=setInterval(function(){
    if(!lastMsg||!ws)return;
    if(Date.now()-lastMsg>WD_MS&&ws.readyState===1){
      setLink(false);
      try{ws.close()}catch(e){}     // onclose stoesst den Neuaufbau an
    }
  },1000);
}
conn();
</script></body></html>
)HTML";
