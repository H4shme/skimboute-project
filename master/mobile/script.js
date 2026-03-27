
// ════════════════════════════════════════════════════════════════
// JS SECTION 1 — ÉTAT GLOBAL / GLOBAL STATE
// FR : Variables partagées dans toute l'application
// EN : Variables shared across the entire application
// ════════════════════════════════════════════════════════════════

let connected   = false;   // FR: Connexion BT active / EN: Active BT connection
let lampOn      = false;   // FR: Lampe allumée / EN: Lamp state
let autoScroll  = true;    // FR: Défilement auto du log / EN: Log auto-scroll
let simInterval = null;    // FR: Timer de simulation / EN: Simulation timer
let btDevice    = null;    // FR: Objet appareil BLE / EN: BLE device object
let btChar      = null;    // FR: Caractéristique BLE / EN: BLE characteristic

const thresholds  = [80, 80, 80, 80]; // FR: Seuils humidité en % / EN: Humidity thresholds in %
const humValues   = [0, 0, 0, 0];     // FR: Valeurs courantes / EN: Current values
const presValues  = [false, false];   // FR: États de présence / EN: Presence states

// ════════════════════════════════════════════════════════════════
// JS SECTION 2 — BLUETOOTH
// FR : Connexion via Web Bluetooth API + fallback simulation
// EN : Connection via Web Bluetooth API + simulation fallback
// ════════════════════════════════════════════════════════════════

async function toggleBT() {
  if (connected) { disconnectBT(); return; }

  // Tentative de connexion Web Bluetooth réelle / Attempt real Web Bluetooth
  if (navigator.bluetooth) {
    try {
      log('Recherche Bluetooth…', 'info');
      btDevice = await navigator.bluetooth.requestDevice({
        acceptAllDevices: true,
        optionalServices: ['0000ffe0-0000-1000-8000-00805f9b34fb']
      });
      btDevice.addEventListener('gattserverdisconnected', onBTDisconnect);

      log('Connexion à ' + btDevice.name + '…', 'info');
      const server  = await btDevice.gatt.connect();
      const service = await server.getPrimaryService('0000ffe0-0000-1000-8000-00805f9b34fb');
      btChar        = await service.getCharacteristic('0000ffe1-0000-1000-8000-00805f9b34fb');

      await btChar.startNotifications();
      btChar.addEventListener('characteristicvaluechanged', onBTData);
      onConnected(btDevice.name);

    } catch(e) {
      log('BT erreur: ' + e.message, 'err');
      startSimulation(); // Fallback simulation
    }
  } else {
    log('Web Bluetooth non dispo → mode simulation', 'warn');
    startSimulation();
  }
}

// FR: Démarre le mode simulation (sans matériel) / EN: Start simulation mode (no hardware)
function startSimulation() {
  onConnected('RPI-Skim [SIM]');
  simInterval = setInterval(simulateTick, 1000);
}

// FR: Génère des données simulées / EN: Generates simulated data
function simulateTick() {
  const l = motorVals['L'];
  const r = motorVals['R'];
  const fake = {
    rpmL: Math.max(0, Math.round(l * 28 + (Math.random()-0.5)*40)),
    rpmR: Math.max(0, Math.round(r * 28 + (Math.random()-0.5)*40)),
    pres: [Math.random() > 0.85, Math.random() > 0.9],
    hum:  humValues.map((v, i) => {
      let n = v + (Math.random()-0.48)*3;
      n = Math.max(20, Math.min(99, n));
      humValues[i] = Math.round(n);
      return humValues[i];
    })
  };
  applyTelemetry(fake);
}

// FR: Reçoit les données BLE brutes et les parse / EN: Receives raw BLE data and parses it
function onBTData(event) {
  const dec = new TextDecoder();
  const raw = dec.decode(event.target.value).trim();
  try {
    const d = JSON.parse(raw);
    applyTelemetry(d);
  } catch(e) {
    log('Parse err: ' + raw, 'err');
  }
}

// FR: Applique les données de télémétrie à l'UI / EN: Applies telemetry data to the UI
function applyTelemetry(d) {
  // RPM moteurs / Motor RPM
  if (d.rpmL !== undefined) document.getElementById('rpmL').textContent = d.rpmL + ' RPM';
  if (d.rpmR !== undefined) document.getElementById('rpmR').textContent = d.rpmR + ' RPM';

  // Présence / Presence
  if (d.pres) {
    d.pres.forEach((v, i) => {
      presValues[i] = v;
      const card  = document.getElementById('pres' + i);
      const state = document.getElementById('presState' + i);
      if (v) {
        card.classList.add('detected');
        state.textContent = 'DÉTECTÉ';
        log('Présence ' + (i+1) + ' détectée', 'warn');
      } else {
        card.classList.remove('detected');
        state.textContent = 'Aucune';
      }
    });
  }

  // Humidité / Humidity
  if (d.hum) {
    let alertList = [];
    d.hum.forEach((v, i) => {
      humValues[i] = v;
      document.getElementById('humVal' + i).innerHTML = v + '<span style="font-size:14px">%</span>';
      document.getElementById('humBar' + i).style.width = Math.min(v, 100) + '%';
      const card  = document.getElementById('humCard' + i);
      const badge = document.getElementById('humBadge' + i);
      if (v >= thresholds[i]) {
        card.classList.add('alert');
        badge.textContent = 'ALERTE';
        alertList.push('Capteur ' + (i+1) + ': ' + v + '% (seuil ' + thresholds[i] + '%)');
      } else {
        card.classList.remove('alert');
        badge.textContent = 'OK';
      }
    });
    if (alertList.length > 0) showAlert(alertList.join('\n'));
  }

  document.getElementById('statusRight').textContent = ts();
}

// FR: Appelée quand la connexion est établie / EN: Called when connection is established
function onConnected(name) {
  connected = true;
  document.getElementById('btBtn').classList.add('on');
  document.getElementById('btDot').classList.add('pulse');
  document.getElementById('btLabel').textContent = name || 'Connecté';
  document.getElementById('btDeviceName').textContent = name || '—';
  ['btnLm','btnLp','btnRm','btnRp'].forEach(id => document.getElementById(id).classList.remove('disabled'));
  document.getElementById('connDot').classList.add('on');
  document.getElementById('connLabel').textContent = 'Connecté';
  log('Connecté: ' + (name || 'inconnu'), 'ok');
}

// FR: Déconnexion manuelle / EN: Manual disconnect
function disconnectBT() {
  if (btDevice && btDevice.gatt.connected) btDevice.gatt.disconnect();
  clearInterval(simInterval);
  onBTDisconnect();
}

// FR: Appelée à la déconnexion (manuelle ou perte de signal) / EN: Called on disconnect (manual or signal loss)
function onBTDisconnect() {
  connected = false;
  document.getElementById('btBtn').classList.remove('on');
  document.getElementById('btDot').classList.remove('pulse');
  document.getElementById('btLabel').textContent = 'Connecter';
  ['btnLm','btnLp','btnRm','btnRp'].forEach(id => document.getElementById(id).classList.add('disabled'));
  document.getElementById('connDot').classList.remove('on');
  document.getElementById('connLabel').textContent = 'Déconnecté';
  document.getElementById('rpmL').textContent = '— RPM';
  document.getElementById('rpmR').textContent = '— RPM';
  log('Déconnecté.', 'err');
}

// ════════════════════════════════════════════════════════════════
// JS SECTION 3 — MOTEURS / MOTORS
// FR : Gestion de la puissance des moteurs gauche et droit
// EN : Left and right motor power management
// ════════════════════════════════════════════════════════════════

let motorVals = { L: 0, R: 0 }; // FR: Puissance 0-100% / EN: Power 0-100%
let holdTimer = null;             // FR: Timer pour appui long / EN: Hold timer

// FR: Démarre une répétition automatique sur appui long / EN: Starts auto-repeat on hold
function startHold(side, delta) {
  stepMotor(side, delta);
  holdTimer = setInterval(() => stepMotor(side, delta), 150);
}
function stopHold() { clearInterval(holdTimer); }

// FR: Incrémente/décrémente la puissance d'un moteur / EN: Increments/decrements a motor's power
function stepMotor(side, delta) {
  motorVals[side] = Math.max(0, Math.min(100, motorVals[side] + delta));
  updateMotorUI(side);
  sendBT({ type: 'motor', side: side, power: motorVals[side] });
}

// FR: Définit directement la valeur d'un moteur / EN: Directly sets a motor value
function setMotor(side, val) {
  motorVals[side] = Math.max(0, Math.min(100, parseInt(val)));
  updateMotorUI(side);
}

// FR: Met à jour l'affichage (% + barre) / EN: Updates the display (% + bar)
function updateMotorUI(side) {
  const v = motorVals[side];
  document.getElementById('pct' + side).textContent = v + '%';
  document.getElementById('bar' + side).style.width = v + '%';
}

// ════════════════════════════════════════════════════════════════
// JS SECTION 4 — LAMPE / LAMP
// FR : Allume/éteint la lampe de surface via BLE
// EN : Turns surface lamp on/off via BLE
// ════════════════════════════════════════════════════════════════

function toggleLamp() {
  if (!connected) return;
  lampOn = !lampOn;
  const tog = document.getElementById('lampToggle');
  const st  = document.getElementById('lampStatus');
  if (lampOn) {
    tog.classList.add('on');
    st.textContent = 'Allumée';
    st.classList.add('on');
  } else {
    tog.classList.remove('on');
    st.textContent = 'Éteinte';
    st.classList.remove('on');
  }
  sendBT({ type: 'lamp', state: lampOn ? 1 : 0 });
  log('Lampe ' + (lampOn ? 'allumée' : 'éteinte'), lampOn ? 'ok' : 'info');
}

// ════════════════════════════════════════════════════════════════
// JS SECTION 5 — COMMANDES / COMMANDS
// FR : Envoi d'une commande groupée + arrêt d'urgence
// EN : Send grouped command + emergency stop
// ════════════════════════════════════════════════════════════════

// FR: Envoie l'état complet (moteurs + lampe) / EN: Sends full state (motors + lamp)
function sendCmd() {
  const l = motorVals['L'];
  const r = motorVals['R'];
  sendBT({ type: 'cmd', motorL: l, motorR: r, lamp: lampOn ? 1 : 0 });
  log('CMD envoyée → L:' + l + '% R:' + r + '%', 'ok');
}

// FR: Coupe tout immédiatement / EN: Cuts everything immediately
function emergencyStop() {
  motorVals['L'] = 0;
  motorVals['R'] = 0;
  updateMotorUI('L');
  updateMotorUI('R');
  sendBT({ type: 'estop' });
  log('ARRÊT D\'URGENCE', 'err');
}

// FR: Envoie un objet JSON via BLE / EN: Sends a JSON object via BLE
function sendBT(obj) {
  if (!btChar) return;
  const enc = new TextEncoder();
  btChar.writeValue(enc.encode(JSON.stringify(obj) + '\n'))
    .catch(e => log('BT write err: ' + e.message, 'err'));
}

// ════════════════════════════════════════════════════════════════
// JS SECTION 6 — ALERTES / ALERTS
// FR : Affichage de l'alerte humidité + vibration
// EN : Humidity alert display + vibration
// ════════════════════════════════════════════════════════════════

let alertActive = false;

// FR: Affiche l'alerte (une seule à la fois) / EN: Shows the alert (one at a time)
function showAlert(msg) {
  if (alertActive) return;
  alertActive = true;
  document.getElementById('alertMsg').textContent = msg;
  document.getElementById('alertOverlay').classList.add('show');
  if (navigator.vibrate) navigator.vibrate([300, 100, 300, 100, 300]);
  log('ALERTE HUMIDITÉ: ' + msg.replace('\n', ' | '), 'err');
}

// FR: Acquitte et ferme l'alerte / EN: Acknowledges and closes the alert
function dismissAlert() {
  alertActive = false;
  document.getElementById('alertOverlay').classList.remove('show');
}

// ════════════════════════════════════════════════════════════════
// JS SECTION 7 — SEUILS D'HUMIDITÉ / HUMIDITY THRESHOLDS
// FR : Mise à jour des seuils depuis la page Réglages
// EN : Update thresholds from the Settings page
// ════════════════════════════════════════════════════════════════

function updateThreshold(i, val) {
  thresholds[i] = parseInt(val);
  document.getElementById('humTh' + i).textContent = 'seuil: ' + thresholds[i] + '%';
  log('Seuil humidité ' + (i+1) + ' → ' + thresholds[i] + '%', 'info');
}

// ════════════════════════════════════════════════════════════════
// JS SECTION 8 — UTILITAIRES UI / UI HELPERS
// FR : Navigation entre pages, log, timestamp
// EN : Page navigation, log, timestamp
// ════════════════════════════════════════════════════════════════

// FR: Change de page en cliquant sur un onglet / EN: Switches page on tab click
function showPage(name) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById('page-' + name).classList.add('active');
  event.target.classList.add('active');
}

// FR: Ajoute une ligne dans le journal / EN: Adds a line in the log
function log(msg, cls) {
  const box = document.getElementById('logBox');
  const el  = document.createElement('div');
  el.className  = 'log-entry ' + (cls || '');
  el.textContent = '[' + ts() + '] ' + msg;
  box.appendChild(el);
  if (autoScroll) box.scrollTop = box.scrollHeight;
}

// FR: Efface le journal / EN: Clears the log
function clearLog() {
  document.getElementById('logBox').innerHTML = '';
  log('Journal effacé', 'info');
}

// FR: Active/désactive le défilement automatique / EN: Toggles auto-scroll
function toggleAutoScroll() {
  autoScroll = !autoScroll;
  document.getElementById('autoScrollBtn').textContent = 'Auto-scroll: ' + (autoScroll ? 'ON' : 'OFF');
}

// FR: Retourne l'heure courante HH:MM:SS / EN: Returns current time HH:MM:SS
function ts() {
  const d = new Date();
  return [d.getHours(), d.getMinutes(), d.getSeconds()]
    .map(n => String(n).padStart(2, '0')).join(':');
}
</script>
</body>
</html>
