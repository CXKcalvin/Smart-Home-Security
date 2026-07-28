const client = mqtt.connect("wss://broker.hivemq.com:8884/mqtt");

const controlTopic = "smarthome/door/control";

const statusTopic = "smarthome/door/status";

client.on("connect", () => {
  document.getElementById("connection").innerHTML = "🟢 ESP32 Terhubung";

  client.subscribe(statusTopic);
});

client.on("offline", () => {
  document.getElementById("connection").innerHTML = "🔴 ESP32 Offline";
});

client.on("message", (topic, message) => {
  let status = message.toString();

  if (status == "ONLINE") {
    return;
  }

  if (status == "OPEN") {
    document.getElementById("doorStatus").innerHTML = "🔓 PINTU TERBUKA";

    document.getElementById("passwordStatus").innerHTML = "✅ PASSWORD BENAR";

    timer();
  }

  if (status == "CLOSED") {
    document.getElementById("doorStatus").innerHTML = "🔒 PINTU TERKUNCI";

    document.getElementById("timer").innerHTML = "-";
  }

  if (status == "PASSWORD_SALAH") {
    document.getElementById("passwordStatus").innerHTML = "❌ PASSWORD SALAH";
  }
});

function sendPassword() {
  let pw = document.getElementById("password").value;

  client.publish(controlTopic, pw);
}

function timer() {
  let t = 15;

  let interval = setInterval(() => {
    document.getElementById("timer").innerHTML = t + " detik";

    t--;

    if (t < 0) {
      clearInterval(interval);
    }
  }, 1000);
}
