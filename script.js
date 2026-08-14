// =============================
// MQTT CONFIGURATION
// =============================
const client = mqtt.connect("wss://broker.hivemq.com:8884/mqtt");

const controlTopic = "smarthome/door/control";
const statusTopic = "smarthome/door/status";

// =============================
// DOM ELEMENTS
// =============================
const connectionEl = document.getElementById("connection");
const doorStatusEl = document.getElementById("doorStatus");
const passwordStatusEl = document.getElementById("passwordStatus");
const passwordInput = document.getElementById("password");

// =============================
// HELPER FUNCTIONS
// =============================
function setConnectionStatus(connected) {
  if (connected) {
    connectionEl.className = "connected";
    connectionEl.innerHTML = '<i class="fas fa-circle"></i> 🟢 ESP32 Terhubung';
  } else {
    connectionEl.className = "";
    connectionEl.innerHTML = '<i class="fas fa-circle"></i> 🔴 ESP32 Offline';
  }
}

function setDoorStatus(status) {
  if (status === "OPEN") {
    doorStatusEl.className = "open";
    doorStatusEl.innerHTML =
      '<i class="fas fa-lock-open"></i> 🔓 PINTU TERBUKA';
  } else if (status === "CLOSED") {
    doorStatusEl.className = "closed";
    doorStatusEl.innerHTML = '<i class="fas fa-lock"></i> 🔒 PINTU TERKUNCI';
  }
}

function setPasswordStatus(status, message) {
  // Reset classes
  passwordStatusEl.className = "";

  if (status === "success") {
    passwordStatusEl.className = "success";
    passwordStatusEl.innerHTML = `<i class="fas fa-check-circle"></i> ✅ ${message}`;
  } else if (status === "error") {
    passwordStatusEl.className = "error";
    passwordStatusEl.innerHTML = `<i class="fas fa-times-circle"></i> ❌ ${message}`;
  } else if (status === "warning") {
    passwordStatusEl.className = "warning";
    passwordStatusEl.innerHTML = `<i class="fas fa-exclamation-triangle"></i> ⚠️ ${message}`;
  } else {
    passwordStatusEl.innerHTML = `<i class="fas fa-minus-circle"></i> ${message || "-"}`;
  }
}

// =============================
// MQTT CONNECT
// =============================
client.on("connect", () => {
  setConnectionStatus(true);
  client.subscribe(statusTopic);
});

// =============================
// MQTT OFFLINE
// =============================
client.on("offline", () => {
  setConnectionStatus(false);
});

// =============================
// MQTT MESSAGE
// =============================
client.on("message", (topic, message) => {
  let status = message.toString();

  // =====================
  // ESP32 ONLINE
  // =====================
  if (status === "ONLINE") {
    return;
  }

  // =====================
  // PINTU TERBUKA
  // =====================
  if (status === "OPEN") {
    setDoorStatus("OPEN");
    setPasswordStatus("success", "PASSWORD BENAR");
  }

  // =====================
  // PINTU TERKUNCI
  // =====================
  if (status === "CLOSED") {
    setDoorStatus("CLOSED");
  }

  // =====================
  // PASSWORD SALAH
  // =====================
  if (status === "PASSWORD_SALAH") {
    setPasswordStatus("error", "PASSWORD SALAH");
  }
});

// =============================
// BUKA PINTU
// =============================
function sendPassword() {
  let pw = passwordInput.value;

  if (pw === "") {
    setPasswordStatus("warning", "Masukkan password");
    return;
  }

  client.publish(controlTopic, pw);
  passwordInput.value = "";
}

// =============================
// TUTUP PINTU
// =============================
function closeDoor() {
  client.publish(controlTopic, "CLOSE");
}

// =============================
// KEYBOARD SHORTCUT
// =============================
passwordInput.addEventListener("keypress", (e) => {
  if (e.key === "Enter") {
    sendPassword();
  }
});

// =============================
// INITIAL STATE
// =============================
setConnectionStatus(false);
setDoorStatus("CLOSED");
setPasswordStatus(null, "-");

console.log("🔐 Smart Home Security siap!");
console.log("📡 Broker: wss://broker.hivemq.com:8884/mqtt");
console.log("📨 Control Topic:", controlTopic);
console.log("📨 Status Topic:", statusTopic);
