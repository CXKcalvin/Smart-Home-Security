// =============================
// MQTT CONFIGURATION
// =============================
const client = mqtt.connect("wss://broker.hivemq.com:8884/mqtt");

const controlTopic =
    "cxkcalvin/smarthome/door/control";

const statusTopic =
    "cxkcalvin/smarthome/door/status";

// =============================
// DOM ELEMENTS
// =============================
const connectionEl = document.getElementById("connection");

const doorStatusEl = document.getElementById("doorStatus");

const passwordStatusEl = document.getElementById("passwordStatus");

const passwordInput = document.getElementById("password");

// =============================
// CONNECTION STATUS
// =============================
function setConnectionStatus(connected) {
  if (connected) {
    connectionEl.className = "connected";

    connectionEl.innerHTML = '<i class="fas fa-circle"></i> 🟢 MQTT Terhubung';
  } else {
    connectionEl.className = "";

    connectionEl.innerHTML = '<i class="fas fa-circle"></i> 🔴 MQTT Offline';
  }
}

// =============================
// DOOR STATUS
// =============================
function setDoorStatus(status) {
  if (status === "OPEN") {
    doorStatusEl.className = "open";

    doorStatusEl.innerHTML =
      '<i class="fas fa-lock-open"></i> 🔓 PINTU TERBUKA';
  } else if (status === "CLOSED") {
    doorStatusEl.className = "closed";

    doorStatusEl.innerHTML = '<i class="fas fa-lock"></i> 🔒 PINTU TERKUNCI';
  } else {
    doorStatusEl.className = "warning";

    doorStatusEl.innerHTML =
      '<i class="fas fa-question-circle"></i> ❓ STATUS TIDAK DIKETAHUI';
  }
}

// =============================
// PASSWORD STATUS
// =============================
function setPasswordStatus(status, message) {
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

  client.subscribe(statusTopic, (error) => {
    if (error) {
      console.error("Gagal subscribe:", error);

      return;
    }

    console.log("Subscribe status berhasil");
  });
});

// =============================
// MQTT OFFLINE
// =============================
client.on("offline", () => {
  setConnectionStatus(false);
});

// =============================
// MQTT ERROR
// =============================
client.on("error", (error) => {
  console.error("MQTT Error:", error);
});

// =============================
// MQTT MESSAGE
// =============================
client.on("message", (topic, message) => {
  if (topic !== statusTopic) {
    return;
  }

  const status = message.toString().trim();

  console.log("STATUS ESP32:", status);

  // =====================
  // ESP32 ONLINE
  // =====================
  if (status === "ONLINE") {
    return;
  }

  // =====================
  // PINTU OPEN
  // =====================
  if (status === "OPEN") {
    setDoorStatus("OPEN");

    setPasswordStatus("success", "AKSES DITERIMA");

    return;
  }

  // =====================
  // PINTU CLOSED
  // =====================
  if (status === "CLOSED") {
    setDoorStatus("CLOSED");

    return;
  }

  // =====================
  // PASSWORD SALAH
  // =====================
  if (status === "PASSWORD_SALAH") {
    setPasswordStatus("error", "PASSWORD SALAH");

    return;
  }

  // =====================
  // RFID DITOLAK
  // =====================
  if (status === "RFID_ACCESS_DENIED") {
    setPasswordStatus("error", "KARTU RFID DITOLAK");

    return;
  }

  // =====================
  // RFID DITERIMA
  // =====================
  if (status === "RFID_ACCESS_GRANTED") {
    setPasswordStatus("success", "KARTU RFID DITERIMA");

    return;
  }
});

// =============================
// PASSWORD
// =============================
function sendPassword() {
  const pw =
    passwordInput.value.trim();

  if (pw === "") {
    setPasswordStatus(
      "warning",
      "Masukkan password"
    );

    return;
  }

  if (!client.connected) {
    setPasswordStatus(
      "error",
      "MQTT tidak terhubung"
    );

    return;
  }

  const command =
    "OPEN:" + pw;

  console.log(
    "Mengirim command:",
    command
  );

  client.publish(
    controlTopic,
    command
  );

  passwordInput.value = "";
}

// =============================
// CLOSE DOOR
// =============================
function closeDoor() {
  if (!client.connected) {
    setPasswordStatus("error", "ESP32 / MQTT offline");

    return;
  }

  console.log("Mengirim perintah CLOSE");

  client.publish(controlTopic, "CLOSE");
}

// =============================
// ENTER
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

setDoorStatus("UNKNOWN");

setPasswordStatus(null, "Menunggu status ESP32...");

console.log("🔐 Smart Home Security siap!");

console.log("📡 Broker:", "wss://broker.hivemq.com:8884/mqtt");

console.log("📨 Control Topic:", controlTopic);

console.log("📨 Status Topic:", statusTopic);
