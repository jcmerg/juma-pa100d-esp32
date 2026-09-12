#pragma once
#include <Arduino.h>

// Dashboard. Grenzen und Farben der Pegelanzeigen:
//   RF    0..150 W, warn ab 100, hoch ab 120
//   VSWR  1..3,     warn ab 1.5, hoch ab 2
//   Temp  20..80,   warn ab 50,  hoch ab 60
//   normal #00b33c, warn #ff9900, hoch #e60000, unbeleuchtet #595959
//
// Alle Texte stehen im Objekt L (de/en). Die Firmware schickt fuer Hinweise nur
// einen Code plus Argument, damit hier nichts Festverdrahtetes uebrig bleibt.
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>JUMA PA-100D</title><style>
:root{--bg:#111;--card:#1e2228;--card2:#191d23;--line:#3a4049;--fg:#eee;--dim:#8b95a3;
--ok:#00b33c;--warn:#ff9900;--bad:#e60000;--off:#595959;--acc:#0eb8c0}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);
font:15px/1.45 -apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica Neue,sans-serif}
.wrap{max-width:1240px;margin:0 auto;padding:16px}
header{display:flex;align-items:center;gap:14px;flex-wrap:wrap;margin-bottom:14px}
h1{font-size:18px;margin:0;font-weight:600;letter-spacing:.3px}
.dot{width:9px;height:9px;border-radius:50%;display:inline-block;background:var(--bad);flex:0 0 auto}
.dot.on{background:var(--ok)}
.pill{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--dim)}
.hbtn{background:#272c34;border:1px solid var(--line);color:var(--fg);border-radius:8px;
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
button{background:#272c34;color:var(--fg);border:1px solid var(--line);border-radius:7px;
padding:9px 14px;font-size:14px;cursor:pointer;font-family:inherit}
button:hover{border-color:var(--acc)}
button:disabled{opacity:.38;cursor:not-allowed}
button:disabled:hover{border-color:var(--line)}
button.act{background:var(--acc);border-color:var(--acc);color:#08191b;font-weight:600}
button.op{background:var(--ok);border-color:var(--ok);color:#03170a;font-weight:600}
button.danger{border-color:#6b2320;color:#f0928d}
.state{font-size:30px;font-weight:700;letter-spacing:1px;line-height:1}
.txb{padding:4px 12px;border-radius:5px;font-size:13px;font-weight:700;background:#2a2f37;color:var(--dim)}
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
.gu{font:400 9px/1 sans-serif;fill:#8b95a3}
.gt{font:400 7px/1 sans-serif;fill:#8b95a3}

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
body.off .wrap{opacity:.4;filter:grayscale(.6)}
.note{font-size:12px;color:var(--dim);margin-top:10px;min-height:1em}
.note.warn{color:var(--warn)}
.sw{display:flex;align-items:center;gap:9px;cursor:pointer;user-select:none}
.sw i{width:42px;height:23px;border-radius:12px;background:#3a4049;position:relative;transition:.15s;flex:0 0 auto}
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
</style></head><body><div class="offline" id="off"></div><div class="wrap">

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
    <div class="hint" data-th="attHint"></div>
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
  </div></div>

  <div class="card"><h2 data-t="opHdr"></h2><div class="tiles">
    <div class="t"><div class="k" data-t="tVolt"></div><div class="v"><span id="v">-</span><small> V</small></div></div>
    <div class="t"><div class="k" data-t="tAmp"></div><div class="v"><span id="a">-</span><small> A</small></div></div>
    <div class="t"><div class="k" data-t="tScale"></div><div class="v" id="scale" style="font-size:16px">-</div></div>
    <div class="t"><div class="k" data-t="tAtt"></div><div class="v" id="gnow" style="font-size:16px">-</div></div>
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

  <div class="sec"><label data-t="lLang"></label>
    <div class="row"><button class="hbtn" id="bDe" style="flex:1">Deutsch</button>
      <button class="hbtn" id="bEn" style="flex:1">English</button></div>
  </div>
</div>

<script>
// --- Texte ----------------------------------------------------------------
const L={
de:{bandHdr:"Band — PA meldet",abLabel:"Bandwahl per TCI",
attHdr:"Abschwächer",attHint:"G1 = 6 dB · G2 = 4 dB · G3 = 2 dB · G4 = 0 dB — laut Manual ein Abschwächer, kein Verstärkungsfaktor. Die PA speichert ihn pro Band.",
lvlHdr:"Pegel",thHdr:"Temperatur & Lüfter",opHdr:"Betriebsdaten",alHdr:"Alarme",
tTemp:"PA Temp",tFan:"Lüfter",tVolt:"Spannung",tAmp:"Strom",tSel:"Bandwahl der PA",
tScale:"Skala",tAtt:"Abschwächer",bClear:"Alarm quittieren",
cfgHdr:"Konfiguration",lSsid:"WLAN SSID",lPass:"WLAN Passwort",phPass:"unverändert lassen",
lTciHost:"TCI Host (SDR-Software)",lTciPort:"TCI Port",lTciOn:"TCI aktiv",lTciLost:"Bei TCI-Verlust auf Automatik der PA",tciLostHint:"Schickt nach 15 s ohne TCI ein =A. Welche Methode die PA dann nutzt, steht in ihrer eigenen Konfiguration (F-Sense, FT-817, Yaesu CAT, KX2/KX3, JUMA-TRX2) — steht sie dort auf Manual, bringt =A nichts. Ohne diesen Schalter bleibt die PA auf dem zuletzt kommandierten Band, weil =Bn sie von A auf M schaltet.",
bSave:"Speichern & neu starten",lFw:"Firmware-Update",bUpload:"Hochladen",
lOff:"Verstärker abschalten",lLang:"Sprache",lOtaStby:"Vor dem Update auf STANDBY",otaStbyHint:"Schickt =S, bevor die neue Firmware geschrieben wird. Während des Schreibens und des Neustarts regelt nichts die PA. Aus lassen, wenn dir Entwicklungs-Uploads nicht die Betriebsart wegnehmen sollen.",
offHint:"Schickt <code>=P0</code> ohne Zustandsspeicherung. Einschalten geht nur am Gerät — zweimal drücken zur Bestätigung.",
fans:["Aus","Langsam","Mittel","Schnell"],
alarms:["SWR zu hoch","Überstrom","Übertemperatur","Überspannung",
        "Unterspannung Vorwarnung","Unterspannung Abschaltung"],
auto:"Automatik",manual:"Manuell",selConflict:"PA wählt selbst, obwohl TCI-Bandwahl an ist",selLocked:"Bei aktiver TCI-Bandwahl bestimmt der ESP32 das Band und hält die PA auf Manuell.",cels:"Celsius",fahr:"Fahrenheit",
tNorm:"normal",tWarm:"warm",tHot:"zu heiß",unitStep:"Stufe",
running:"Läuft: ",noFile:"keine Datei gewählt",loading:"lade %s kB…",
upOk:"OK — Gerät startet neu",upErr:"Fehler %s",upAbort:"Übertragung abgebrochen",
saving:"speichere…",restarting:"Gerät startet neu",saved:"Gespeichert — Gerät startet neu",wsLost:"Verbindung zum Gerät unterbrochen — versuche erneut…",
confirm:"Wirklich? Nochmal drücken",
nAboff:"TCI-Bandwahl aus — die PA folgt der SDR-Software nicht",
nTcioff:"TCI-Client ist abgeschaltet",
nTcidis:"TCI nicht verbunden",nTciauto:"TCI weg — PA auf eigene Bandwahl (=A) zurückgestellt",nSelstuck:"PA bleibt auf eigener Bandwahl, obwohl die TCI-Bandwahl sie auf Manuell holen will",
nTcinofreq:"TCI verbunden, aber noch keine QRG empfangen",
nTxwait:"Bandwechsel wartet: TX aktiv",
nPaoff:"PA antwortet nicht",
nUnsupported:"QRG %s — die PA-100D deckt das Band nicht ab, Band bleibt unverändert",
nBandok:"Band folgt TCI: %s",
nBandset:"Band umgeschaltet: %s"},

en:{bandHdr:"Band — PA reports",abLabel:"Band select via TCI",
attHdr:"Attenuator",attHint:"G1 = 6 dB · G2 = 4 dB · G3 = 2 dB · G4 = 0 dB — per the manual this is an attenuator, not a gain factor. The PA stores it per band.",
lvlHdr:"Levels",thHdr:"Temperature & fan",opHdr:"Operating data",alHdr:"Alarms",
tTemp:"PA temp",tFan:"Fan",tVolt:"Voltage",tAmp:"Current",tSel:"PA band select",
tScale:"Scale",tAtt:"Attenuator",bClear:"Clear alarm",
cfgHdr:"Setup",lSsid:"Wi-Fi SSID",lPass:"Wi-Fi password",phPass:"leave unchanged",
lTciHost:"TCI host (SDR software)",lTciPort:"TCI port",lTciOn:"TCI enabled",lTciLost:"Fall back to the PA\u2019s own band select",tciLostHint:"Sends =A after 15 s without TCI. Which method the PA then uses is set in its own configuration (F-Sense, FT-817, Yaesu CAT, KX2/KX3, JUMA-TRX2) — if that is set to Manual, =A achieves nothing. Without this switch the PA stays on the last commanded band, because =Bn moves it from A to M.",
bSave:"Save & restart",lFw:"Firmware update",bUpload:"Upload",
lOff:"Power down amplifier",lLang:"Language",lOtaStby:"Standby before update",otaStbyHint:"Sends =S before the new firmware is written. Nothing controls the PA while writing and rebooting. Turn off if development uploads should not take away the operating state.",
offHint:"Sends <code>=P0</code> without saving state. Powering on is only possible at the unit — press twice to confirm.",
fans:["Off","Slow","Medium","Fast"],
alarms:["High SWR","Over-current","High temperature","High voltage",
        "Low voltage pre-limit","Low voltage final limit"],
auto:"Automatic",manual:"Manual",selConflict:"PA selects on its own while TCI band select is on",selLocked:"With TCI band select on, the ESP32 determines the band and holds the PA on Manual.",cels:"Celsius",fahr:"Fahrenheit",
tNorm:"normal",tWarm:"warm",tHot:"too hot",unitStep:"Step",
running:"Running: ",noFile:"no file selected",loading:"uploading %s kB…",
upOk:"OK — device restarting",upErr:"Error %s",upAbort:"transfer aborted",
saving:"saving…",restarting:"device restarting",saved:"Saved — device restarting",wsLost:"Connection to the device lost — retrying…",
confirm:"Confirm? Press again",
nAboff:"TCI band select off — the PA does not follow the SDR software",
nTcioff:"TCI client is disabled",
nTcidis:"TCI not connected",nTciauto:"TCI lost — PA switched back to its own band select (=A)",nSelstuck:"PA stays on its own band select although TCI band select wants it on Manual",
nTcinofreq:"TCI connected, but no frequency received yet",
nTxwait:"Band change waiting: TX active",
nPaoff:"PA not responding",
nUnsupported:"QRG %s — the PA-100D does not cover this band, band left unchanged",
nBandok:"Band follows TCI: %s",
nBandset:"Band switched: %s"}};

let lang = localStorage.getItem("lang") ||
  ((navigator.language||"").toLowerCase().indexOf("de")===0 ? "de" : "en");
function t(k,a){const v=L[lang][k];return (a===undefined)?v:String(v).replace("%s",a)}

const BANDS=[[1,"160m"],[2,"80m"],[3,"40m"],[4,"30m"],[5,"20m"],[6,"17m"],[7,"15m"],[8,"12m"],[9,"10m"]];
const AMASK=[1,2,4,8,16,32];
const C={ok:"#00b33c",warn:"#ff9900",bad:"#e60000",off:"#595959"};
const $=i=>document.getElementById(i);
let ws,st={},offArm=0;

// --- Pegelbalken ----------------------------------------------------------
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
// Farbe nach der eigenen Position des Segments - der Balken zeigt so die Zonen,
// statt bei Ueberschreitung komplett umzuschlagen.
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
mkLevel($("lRf"), {label:"RF",   min:0, max:150, warn:100, high:120, unit:" W"});
mkLevel($("lSwr"),{label:"VSWR", min:1, max:3,   warn:1.5, high:2,   unit:""});

// --- Rundanzeigen: 180-Grad-Bogen mit Zonenfarben -------------------------
const GA1=180,GR=40,GCX=50,GCY=47;
function pol(r,deg){const a=(deg-180)*Math.PI/180;return[GCX+r*Math.cos(a),GCY+r*Math.sin(a)]}
function arcPath(r,d0,d1){
  const p0=pol(r,d0),p1=pol(r,d1);
  return "M"+p0[0].toFixed(2)+" "+p0[1].toFixed(2)+" A"+r+" "+r+" 0 "+
         ((d1-d0)>180?1:0)+" 1 "+p1[0].toFixed(2)+" "+p1[1].toFixed(2);
}
function mkGauge(el,zones,unit,ticks){
  let g='<svg viewBox="-10 -10 120 74">';
  zones.forEach((z,i)=>{
    const f0=i?zones[i-1][0]:0;
    g+='<path d="'+arcPath(GR,f0*GA1,z[0]*GA1)+'" fill="none" stroke="'+z[1]+
       '" stroke-opacity=".22" stroke-width="11"/>';
  });
  g+='<path class="gval" d="" fill="none" stroke="#595959" stroke-width="11"/>';
  (ticks||[]).forEach(function(tk){
    const p=pol(GR+11,tk[0]*GA1);
    g+='<text class="gt" x="'+p[0].toFixed(1)+'" y="'+p[1].toFixed(1)+
       '" text-anchor="middle" dominant-baseline="middle">'+tk[1]+'</text>';
  });
  g+='<text class="gv" x="50" y="44" text-anchor="middle" fill="#595959">-</text>'+
     '<text class="gu" x="50" y="55" text-anchor="middle">'+unit+'</text></svg>';
  el.innerHTML=g;
}
function setGauge(el,frac,text,color){
  const sv=el.querySelector("svg"); if(!sv)return;
  const val=sv.querySelector(".gval"),num=sv.querySelector(".gv");
  const f=Math.max(0,Math.min(1,frac||0));
  val.setAttribute("d", f<=0.001 ? "" : arcPath(GR,0,f*GA1));
  val.setAttribute("stroke",color);
  num.textContent=text;
  num.setAttribute("fill",color);
}
const TMIN=20,TMAX=80,TWARN=50,THIGH=60;
const FW=(TWARN-TMIN)/(TMAX-TMIN), FH=(THIGH-TMIN)/(TMAX-TMIN);
function buildGauges(){
  mkGauge($("gTmp"),[[FW,C.ok],[FH,C.warn],[1,C.bad]],
          "°C",[[0,"20"],[FW,"50"],[FH,"60"],[1,"80"]]);
  mkGauge($("gFan"),[[.25,C.off],[.5,C.ok],[.75,C.warn],[1,C.bad]],
          t("unitStep"),[[.125,"0"],[.375,"1"],[.625,"2"],[.875,"3"]]);
}

// --- Bedienelemente -------------------------------------------------------
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
// "Auto" = =A. "Manuell" hat kein eigenes Kommando - die Firmware schickt
// dafuer das Band, auf dem die PA schon steht.
$("bSelM").onclick=function(){send("bandsel",0)};
$("bSelA").onclick=function(){send("bandsel",1)};
$("tcien").onclick=function(e){e.currentTarget.classList.toggle("on")};
$("otastby").onclick=function(e){e.currentTarget.classList.toggle("on")};
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
f.set("otastby",$("otastby").classList.contains("on")?"1":"0");
f.set("tcilosta",$("tcilosta").classList.contains("on")?"1":"0");
$("cfgSt").textContent=t("saving");
fetch("/api/config",{method:"POST",body:new URLSearchParams(f)})
.then(function(){$("cfgSt").textContent=t("saved")})
.catch(function(){$("cfgSt").textContent=t("restarting")})};

$("bFw").onclick=function(){const f=$("fw").files[0];
if(!f){$("fwSt").textContent=t("noFile");return}
const fd=new FormData();fd.append("firmware",f,f.name);
$("fwSt").textContent=t("loading",f.size/1024|0);
const x=new XMLHttpRequest();x.open("POST","/update",true);
x.upload.onprogress=function(e){if(e.lengthComputable)
$("fwSt").textContent=Math.round(e.loaded/e.total*100)+" %"};
x.onload=function(){$("fwSt").textContent=x.status==200?t("upOk"):t("upErr",x.status)};
x.onerror=function(){$("fwSt").textContent=t("upAbort")};x.send(fd)};

function send(c,v){if(ws&&ws.readyState==1)ws.send(c+":"+v)}

// --- Sprache --------------------------------------------------------------
function setLang(l){lang=l;localStorage.setItem("lang",l);applyLang()}
$("bDe").onclick=function(){setLang("de")};
$("bEn").onclick=function(){setLang("en")};

function applyLang(){
  document.documentElement.lang=lang;
  document.querySelectorAll("[data-t]").forEach(function(e){e.textContent=t(e.dataset.t)});
  document.querySelectorAll("[data-th]").forEach(function(e){e.innerHTML=t(e.dataset.th)});
  document.querySelectorAll("[data-tp]").forEach(function(e){e.placeholder=t(e.dataset.tp)});
  document.querySelectorAll("#alarms div").forEach(function(d){
    d.querySelector(".an").textContent=t("alarms")[+d.dataset.i]});
  $("bDe").className="hbtn"+(lang==="de"?" act":"");
  $("bEn").className="hbtn"+(lang==="en"?" act":"");
  buildGauges();
  if($("off").className.indexOf("on")>=0)$("off").textContent=t("wsLost");
  if(st.raw!==undefined)render(st);
}

// --- Zustand anzeigen -----------------------------------------------------
// Nur echte Hindernisse werden als Warnung eingefaerbt - "Automatik aus" oder
// "Band folgt TCI" sind Zustandsinfos, keine Probleme.
const NWARN={tcidis:1,tcinofreq:1,txwait:1,unsupported:1,paoff:1,selstuck:1};
function noteText(s){
  if(!s.note)return "";
  const k="n"+s.note.charAt(0).toUpperCase()+s.note.slice(1);
  return L[lang][k] ? t(k,s.noteArg||"") : s.note;
}
function render(s){st=s;
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
  const tp=s.temp,tc=tp>=THIGH?C.bad:tp>=TWARN?C.warn:C.ok;
  setGauge($("gTmp"),(tp-TMIN)/(TMAX-TMIN),String(tp),tc);
  $("gTmpS").textContent=(s.celsius?t("cels"):t("fahr"))+" · "+
    (tp>=THIGH?t("tHot"):tp>=TWARN?t("tWarm"):t("tNorm"));
  $("gTmpS").style.color=tc;
  const fc=[C.off,C.ok,C.warn,C.bad][s.fan]||C.off;
  setGauge($("gFan"),(s.fan+1)/4,String(s.fan),fc);
  $("gFanS").textContent=t("fans")[s.fan]||"?";
  $("gFanS").style.color=s.fan?fc:"var(--dim)";
}else{
  setGauge($("gTmp"),0,"-",C.off);$("gTmpS").innerHTML="&nbsp;";
  setGauge($("gFan"),0,"-",C.off);$("gFanS").innerHTML="&nbsp;";
}

$("v").textContent=s.online?s.volts.toFixed(2):"-";
$("a").textContent=s.online?s.amps.toFixed(1):"-";
// In Automatik bestimmt die PA das Band per F-Sense selbst und kann damit
// gegen unsere =Bn arbeiten - das ist nur ein Problem, wenn TCI-Bandwahl laeuft.
$("bSelM").className="hbtn"+(s.online&&!s.autoSel?" act":"");
$("bSelA").className="hbtn"+(s.online&&s.autoSel?" act":"");
// Bei aktiver TCI-Bandwahl bestimmt der ESP32 das Band - eine Umschaltung
// waere beim naechsten =Bn sofort wieder weg.
const locked=s.autoband||!s.online;
$("bSelM").disabled=locked;$("bSelA").disabled=locked;
const conflict=s.online&&s.autoSel&&s.autoband;
$("aselSub").textContent=conflict?t("selConflict"):(s.autoband?t("selLocked"):"");
$("aselSub").style.color=conflict?C.warn:"var(--dim)";
$("scale").textContent=s.online?(s.celsius?t("cels"):t("fahr")):"-";
// Welche Stufe aktiv ist, zeigen die G-Tasten in der linken Spalte - hier
// interessiert nur der Wert.
$("gnow").textContent=s.online&&s.gain?[6,4,2,0][s.gain-1]+" dB":"-";

$("bandNow").textContent=s.bandName;
document.querySelectorAll("#bands button").forEach(function(b){
b.className=(+b.dataset.b===s.band)?"act":""});
document.querySelectorAll("#gains button").forEach(function(b){
b.className=(+b.dataset.g===s.gain)?"act":""});
document.querySelectorAll("#alarms div").forEach(function(d){
const hit=(s.alarms&+d.dataset.m)!==0;
d.className=hit?"hit":"";d.firstChild.style.background=hit?C.bad:"#3a4049"});

$("ab").className="sw"+(s.autoband?" on":"");
$("raw").textContent=s.raw;
$("note").textContent=noteText(s);
$("note").className="note"+(NWARN[s.note]?" warn":"");
$("fwSt").textContent=t("running")+(s.version||"");
if(!$("panel").classList.contains("on")){
  $("tcihost").value=s.tciHost;$("tciport").value=s.tciPort;$("ssid").value=s.ssid;
  $("tcien").className="sw"+(s.tciEnabled?" on":"");
  $("otastby").className="sw"+(s.otaStandby?" on":"");
  $("tcilosta").className="sw"+(s.tciLostAuto?" on":"")}}

applyLang();
// Ein stehengebliebenes Dashboard mit alten Werten sieht aus wie ein Hänger -
// deshalb sagt die Seite deutlich, wenn die Verbindung weg ist.
function setLink(on){
  $("off").className="offline"+(on?"":" on");
  document.body.className=on?"":"off";
  if(!on)$("off").textContent=t("wsLost");
}
function conn(){ws=new WebSocket("ws://"+location.hostname+":81/");
ws.onopen=function(){setLink(true)};
ws.onmessage=function(e){render(JSON.parse(e.data))};
ws.onclose=function(){$("dPa").className="dot";setLink(false);setTimeout(conn,2000)};
ws.onerror=function(){setLink(false)}}
conn();
</script></body></html>
)HTML";
