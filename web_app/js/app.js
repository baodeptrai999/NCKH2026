// Firebase config
const firebaseConfig = {
  apiKey: "AIzaSyCm0oE0pS3d3Ak7s0qyx-zm5IS0TVsWsK0",
  authDomain: "baochay-e788a.firebaseapp.com",
  databaseURL: "https://baochay-e788a-default-rtdb.firebaseio.com",
  projectId: "baochay-e788a",
  storageBucket: "baochay-e788a.firebasestorage.app",
  messagingSenderId: "103010478177",
  appId: "1:103010478177:web:cd3421b65e079995b7e220"
};
firebase.initializeApp(firebaseConfig);
const db = firebase.database();
const auth = firebase.auth();

// Auth guard
auth.onAuthStateChanged(user => {
  if (!user) {
    window.location.href = 'login.html';
    return;
  }
  const el = document.getElementById('userInitial');
  if (el) el.textContent = (user.displayName || user.email || 'U')[0].toUpperCase();
  const nameEl = document.getElementById('userName');
  if (nameEl) nameEl.textContent = user.displayName || user.email;
  initDashboard();
});

function logout() {
  auth.signOut().then(() => window.location.href = 'login.html');
}

// Clock
function updateClock() {
  const t = new Date().toTimeString().slice(0, 8);
  const el = document.getElementById('clock');
  if (el) el.textContent = t;
  const cam = document.getElementById('camTs');
  if (cam) cam.textContent = 'CAM-01 | ' + t;
  const camFull = document.getElementById('camTsFull');
  if (camFull) camFull.textContent = 'CAM-01 | FULLSCREEN | ' + t;
}

// Log system
const logEntries = [];
function addLog(msg, type) {
  const time = new Date().toTimeString().slice(0, 8);
  logEntries.unshift({ time, msg, type });
  if (logEntries.length > 50) logEntries.pop();
  renderLogs();
}

function renderLogs() {
  const container = document.getElementById('logEntries');
  if (!container) return;
  container.innerHTML = logEntries.slice(0, 15).map(l =>
    `<div class="log-entry"><span class="time">${l.time}</span><span class="msg ${l.type}">${l.msg}</span></div>`
  ).join('');
}

// Notification system
const notifications = [];
function addNotification(title, desc, type) {
  const time = new Date().toLocaleString('vi-VN');
  notifications.unshift({ title, desc, time, type, unread: true });
  updateNotifBadge();

  // Browser notification
  if (Notification.permission === 'granted' && type === 'danger') {
    new Notification('🔥 FireSentinel Alert', { body: title, icon: '🔥' });
  }
}

function updateNotifBadge() {
  const dot = document.getElementById('notifDot');
  const unread = notifications.filter(n => n.unread).length;
  if (dot) dot.style.display = unread > 0 ? 'block' : 'none';
}

function toggleNotifPanel() {
  const panel = document.getElementById('notifPanel');
  const overlay = document.getElementById('overlay');
  const isOpen = panel.classList.contains('open');
  panel.classList.toggle('open');
  overlay.classList.toggle('show');
  if (!isOpen) {
    notifications.forEach(n => n.unread = false);
    updateNotifBadge();
    renderNotifications();
  }
}

function renderNotifications() {
  const container = document.getElementById('notifList');
  if (!container) return;
  if (notifications.length === 0) {
    container.innerHTML = '<div class="notif-empty"><i class="fas fa-bell-slash"></i><p>Chưa có thông báo</p></div>';
    return;
  }
  container.innerHTML = notifications.map(n =>
    `<div class="notif-item ${n.unread ? 'unread' : ''}">
      <div class="notif-title">${n.type === 'danger' ? '🔴' : n.type === 'success' ? '🟢' : '🟡'} ${n.title}</div>
      <div class="notif-desc">${n.desc}</div>
      <div class="notif-time">${n.time}</div>
    </div>`
  ).join('');
}

// Sensor data handling
let currentFireState = false;
let currentGasAlert = false;
let isMQ2Fault = false;
let isDHTFault = false;
let isPreAlarm = false;
let tempRateVal = 0.0;
let cameraIP = "";

function initDashboard() {
  setInterval(updateClock, 1000);
  updateClock();

  // Request notification permission
  if ('Notification' in window && Notification.permission === 'default') {
    Notification.requestPermission();
  }

  // Lắng nghe IP Camera động từ Firebase
  db.ref('Camera_IP').on('value', s => {
    const ip = s.val();
    if (ip) {
      cameraIP = ip;

      // Cập nhật cấu hình ở cài đặt
      const sCamIP = document.getElementById('sCamIP');
      if (sCamIP) sCamIP.textContent = ip;
      const inputCamIP = document.getElementById('inputCamIP');
      if (inputCamIP) inputCamIP.placeholder = ip;

      // Dashboard camera
      const camImg = document.getElementById('esp32cam-img');
      const fallback = document.getElementById('esp32cam-fallback');
      if (camImg) {
        camImg.src = `http://${ip}:81/stream`;
        if (fallback) fallback.style.display = 'none';
        camImg.onerror = () => {
          if (fallback) {
            fallback.style.display = 'flex';
            fallback.textContent = '📷 Lỗi kết nối camera';
          }
        };
      }

      // Camera page (full screen)
      const camImgFull = document.getElementById('esp32cam-img-full');
      const fallbackFull = document.getElementById('esp32cam-fallback-full');
      if (camImgFull) {
        camImgFull.src = `http://${ip}:81/stream`;
        if (fallbackFull) fallbackFull.style.display = 'none';
        camImgFull.onerror = () => {
          if (fallbackFull) {
            fallbackFull.style.display = 'flex';
            fallbackFull.textContent = '📷 Lỗi kết nối camera';
          }
        };
      }

      addLog(`Đã phát hiện ESP32-CAM tại IP: ${ip}`, 'info');
    } else {
      const fallback = document.getElementById('esp32cam-fallback');
      if (fallback) {
        fallback.style.display = 'flex';
        fallback.textContent = '📷 Đang chờ kết nối camera...';
      }
      const fallbackFull = document.getElementById('esp32cam-fallback-full');
      if (fallbackFull) {
        fallbackFull.style.display = 'flex';
        fallbackFull.textContent = '📷 Đang chờ kết nối camera...';
      }
    }
  });

  // Temperature
  db.ref('Nhiet_Do').on('value', s => {
    const v = s.val() || 0;
    if (isDHTFault) {
      document.getElementById('valTemp').textContent = 'ERR';
      document.getElementById('barTemp').style.width = '0%';
    } else {
      document.getElementById('valTemp').textContent = v;
      document.getElementById('barTemp').style.width = Math.min(100, v) + '%';
    }
    const card = document.getElementById('cardTemp');
    const tag = document.getElementById('tagTemp');
    const isAlert = v > 50 && !isDHTFault;
    card.classList.toggle('alert', isAlert);
    tag.className = 'sc-tag ' + (isDHTFault ? 'alert' : isAlert ? 'alert' : 'ok');
    tag.textContent = isDHTFault ? 'HỎNG DÂY' : isAlert ? 'CẢNH BÁO' : 'BÌNH THƯỜNG';
    updateFloorPlan();
  });

  // Humidity
  db.ref('Do_Am').on('value', s => {
    const v = s.val() || 0;
    if (isDHTFault) {
      document.getElementById('valHum').textContent = 'ERR';
      document.getElementById('barHum').style.width = '0%';
    } else {
      document.getElementById('valHum').textContent = v;
      document.getElementById('barHum').style.width = Math.min(100, v) + '%';
    }
    updateFloorPlan();
  });

  // Gas
  db.ref('Khoi_Gas').on('value', s => {
    const v = s.val() || 0;
    if (isMQ2Fault) {
      document.getElementById('valGas').textContent = 'ERR';
      document.getElementById('barGas').style.width = '0%';
    } else {
      document.getElementById('valGas').textContent = v;
      document.getElementById('barGas').style.width = Math.min(100, v / 25) + '%';
    }
    const card = document.getElementById('cardGas');
    const tag = document.getElementById('tagGas');
    const isAlert = v > 100 && !isMQ2Fault; // Ngưỡng khói thực tế
    card.classList.toggle('alert', isAlert);
    tag.className = 'sc-tag ' + (isMQ2Fault ? 'alert' : isAlert ? 'alert' : 'ok');
    tag.textContent = isMQ2Fault ? 'HỎNG DÂY' : isAlert ? 'CẢNH BÁO' : 'BÌNH THƯỜNG';
    updateFloorPlan();
  });

  // Fire state
  db.ref('Trang_Thai_Chay').on('value', s => {
    const on = s.val() === 1;
    document.getElementById('toggle').classList.toggle('on', on);
    currentFireState = on;
    updateStatusDisplay();
  });

  // Gas alert state
  db.ref('Trang_Thai_Gas').on('value', s => {
    const on = s.val() === 1;
    currentGasAlert = on;
    updateStatusDisplay();
  });

  // Lắng nghe lỗi cảm biến MQ2 từ ESP32
  db.ref('Loi_MQ2').on('value', s => {
    const v = s.val() === 1;
    if (v !== isMQ2Fault) {
      isMQ2Fault = v;
      if (v) {
        addLog('⚠️ Cảnh báo: Lỗi cảm biến MQ-2 (Đứt dây/Hỏng)', 'err');
        addNotification('Sự cố phần cứng', 'Cảm biến khí Gas/Khói gặp sự cố đứt dây!', 'warning');
      } else {
        addLog('Cảm biến MQ-2 đã kết nối lại bình thường', 'ok');
      }
      // Refresh dữ liệu khói
      db.ref('Khoi_Gas').once('value', snap => {
        const val = snap.val() || 0;
        const card = document.getElementById('cardGas');
        const tag = document.getElementById('tagGas');
        if (v) {
          document.getElementById('valGas').textContent = 'ERR';
          document.getElementById('barGas').style.width = '0%';
          card.classList.add('alert');
          tag.className = 'sc-tag alert';
          tag.textContent = 'HỎNG DÂY';
        } else {
          document.getElementById('valGas').textContent = val;
          document.getElementById('barGas').style.width = Math.min(100, val / 25) + '%';
          const isAlert = val > 100;
          card.classList.toggle('alert', isAlert);
          tag.className = 'sc-tag ' + (isAlert ? 'alert' : 'ok');
          tag.textContent = isAlert ? 'CẢNH BÁO' : 'BÌNH THƯỜNG';
        }
      });
    }
  });

  // Lắng nghe lỗi cảm biến DHT từ ESP8266
  db.ref('Loi_DHT').on('value', s => {
    const v = s.val() === 1;
    if (v !== isDHTFault) {
      isDHTFault = v;
      if (v) {
        addLog('⚠️ Cảnh báo: Lỗi cảm biến DHT11 (Đứt dây/Hỏng)', 'err');
        addNotification('Sự cố phần cứng', 'Cảm biến nhiệt ẩm phòng phụ gặp sự cố!', 'warning');
      } else {
        addLog('Cảm biến DHT11 đã kết nối lại bình thường', 'ok');
      }
      // Refresh dữ liệu nhiệt độ
      db.ref('Nhiet_Do').once('value', snap => {
        const val = snap.val() || 0;
        const card = document.getElementById('cardTemp');
        const tag = document.getElementById('tagTemp');
        if (v) {
          document.getElementById('valTemp').textContent = 'ERR';
          document.getElementById('barTemp').style.width = '0%';
          card.classList.add('alert');
          tag.className = 'sc-tag alert';
          tag.textContent = 'HỎNG DÂY';
        } else {
          document.getElementById('valTemp').textContent = val;
          document.getElementById('barTemp').style.width = Math.min(100, val) + '%';
          const isAlert = val > 50;
          card.classList.toggle('alert', isAlert);
          tag.className = 'sc-tag ' + (isAlert ? 'alert' : 'ok');
          tag.textContent = isAlert ? 'CẢNH BÁO' : 'BÌNH THƯỜNG';
        }
      });
    }
  });

  // Lắng nghe tốc độ biến thiên nhiệt độ (dT/dt)
  db.ref('Toc_Do_Nhiet').on('value', s => {
    tempRateVal = s.val() || 0.0;
  });

  // Lắng nghe trạng thái Tiền cảnh báo (Pre-alarm)
  db.ref('Tien_Canh_Bao').on('value', s => {
    const on = s.val() === 1;
    if (on !== isPreAlarm) {
      isPreAlarm = on;
      if (on) {
        addLog('⚠️ Cảnh báo sớm: Nhiệt độ tăng nhanh đột ngột!', 'wrn');
        addNotification('Tiền cảnh báo hỏa hoạn', 'Nhiệt độ phòng ngủ tăng nhanh bất thường, nghi ngờ cháy!', 'warning');
      }
      updateStatusDisplay();
    }
  });

  // Lắng nghe trạng thái Máy bơm (Relay 2)
  db.ref('Relay2').on('value', s => {
    const on = s.val() === 1;
    const btn = document.getElementById('togglePump');
    if (btn) btn.classList.toggle('on', on);
    const badge = document.getElementById('badge-pump');
    if (badge) {
      badge.className = on ? "device-badge active" : "device-badge";
      if (on) {
        badge.style.color = "#3b82f6";
        badge.style.borderColor = "rgba(59,130,246,0.3)";
      } else {
        badge.style.color = "";
        badge.style.borderColor = "";
      }
    }
  });

  // Lắng nghe trạng thái Quạt hút (Relay 1)
  db.ref('Relay1').on('value', s => {
    const on = s.val() === 1;
    const btn = document.getElementById('toggleFan');
    if (btn) btn.classList.toggle('on', on);
    const badge = document.getElementById('badge-fan');
    if (badge) {
      badge.className = on ? "device-badge active" : "device-badge";
      if (on) {
        badge.style.color = "#10b981";
        badge.style.borderColor = "rgba(16,185,129,0.3)";
      } else {
        badge.style.color = "";
        badge.style.borderColor = "";
      }
    }
  });

  // Lắng nghe trạng thái Cửa sổ
  db.ref('Cua_So').on('value', s => {
    const open = s.val() === 1;
    const planWindow = document.getElementById('planWindow');
    if (planWindow) planWindow.textContent = open ? "Mở (100%)" : "Đóng";
    const btn = document.getElementById('toggleWindow');
    if (btn) btn.classList.toggle('on', open);
  });

  addLog('Hệ thống đã khởi động thành công', 'ok');
  addLog('Đã kết nối Firebase Realtime Database', 'info');
  addNotification('Hệ thống khởi động', 'FireSentinel đã sẵn sàng giám sát', 'success');
}

function updateStatusDisplay() {
  const on = currentFireState;
  const gas = currentGasAlert;
  const pre = isPreAlarm;

  document.body.classList.toggle('fire-active', on || gas);

  const banner = document.getElementById('statusBanner');
  const statusIcon = document.getElementById('statusIcon');
  const statusTitle = document.getElementById('statusTitle');
  const statusSub = document.getElementById('statusSub');
  const tag = document.getElementById('statusTag');
  const badge = document.getElementById('topBadge');

  const aiText = document.getElementById('aiText');
  const aiPct = document.getElementById('aiPct');

  if (on) {
    banner.className = 'hero danger';
    statusIcon.textContent = '🔥';
    statusTitle.textContent = '🔥 PHÁT HIỆN HỎA HOẠN!';
    statusSub.textContent = 'Mạch ESP32 đã kích hoạt còi và hệ thống chữa cháy tại chỗ!';
    tag.className = 'hero-tag danger';
    tag.textContent = '⚠ ALERT';

    badge.className = 'live-badge fire';
    badge.innerHTML = '<span class="live-dot"></span>FIRE DETECTED';

    if (aiText) aiText.innerHTML = 'AI phán đoán: <strong>CÓ HỎA HOẠN THẬT (95%)</strong>';
    if (aiPct) aiPct.textContent = '95%';

    // Rung điện thoại nếu có hỏa hoạn
    if (navigator.vibrate) navigator.vibrate([500, 200, 500, 200, 500]);

  } else if (gas) {
    banner.className = 'hero danger';
    statusIcon.textContent = '⚠️';
    statusTitle.textContent = '⚠️ PHÁT HIỆN RÒ RỈ GAS!';
    statusSub.textContent = 'Mạch ESP32 đã kích hoạt còi báo động và quạt hút thông gió!';
    tag.className = 'hero-tag danger';
    tag.textContent = '⚠ GAS LEAK';

    badge.className = 'live-badge warning';
    badge.innerHTML = '<span class="live-dot"></span>GAS DETECTED';

    if (aiText) aiText.innerHTML = 'AI phán đoán: <strong>RÒ RỈ KHÍ GAS NGUY HIỂM (90%)</strong>';
    if (aiPct) aiPct.textContent = '90%';

    // Rung điện thoại kiểu cảnh báo gas
    if (navigator.vibrate) navigator.vibrate([300, 100, 300, 100, 300]);

  } else if (pre) {
    banner.className = 'hero warning';
    statusIcon.textContent = '⚠️';
    statusTitle.textContent = '⚠️ TIỀN CẢNH BÁO';
    statusSub.textContent = `Nhiệt độ tăng nhanh bất thường (+${(tempRateVal * 60).toFixed(1)}°C/phút). Nghi ngờ cháy sớm!`;
    tag.className = 'hero-tag warning';
    tag.textContent = '⚠ PRE-ALARM';

    badge.className = 'live-badge warning';
    badge.innerHTML = '<span class="live-dot"></span>PRE-ALARM';

    if (aiText) aiText.innerHTML = 'AI: <strong>Phát hiện nhiệt độ tăng đột ngột (75%)</strong>';
    if (aiPct) aiPct.textContent = '75%';

  } else {
    banner.className = 'hero safe';
    statusIcon.textContent = '🛡️';
    statusTitle.textContent = 'HỆ THỐNG AN TOÀN';
    statusSub.textContent = 'Tất cả cảm biến hoạt động bình thường';
    tag.className = 'hero-tag safe';
    tag.textContent = '✓ NORMAL';

    badge.className = 'live-badge live';
    badge.innerHTML = '<span class="live-dot"></span>LIVE';

    if (aiText) aiText.innerHTML = 'AI: <strong>Đang giám sát...</strong>';
    if (aiPct) aiPct.textContent = '—';
  }
  
  // Cập nhật Sơ đồ mặt bằng realtime
  updateFloorPlan();
}

function updateFloorPlan() {
  const planGas = document.getElementById('planGas');
  const planTemp = document.getElementById('planTemp');
  const planHum = document.getElementById('planHum');
  const planWindow = document.getElementById('planWindow');
  const badgePump = document.getElementById('badge-pump');
  const badgeFan = document.getElementById('badge-fan');
  const badgeTempTrend = document.getElementById('badge-temp-trend');
  
  const zoneLiving = document.getElementById('zone-living');
  const zoneBed = document.getElementById('zone-bed');
  const zoneCorridor = document.getElementById('zone-corridor');

  // 1. Cập nhật các chỉ số cảm biến
  const valGas = document.getElementById('valGas');
  const valTemp = document.getElementById('valTemp');
  const valHum = document.getElementById('valHum');

  if (planGas && valGas) {
    planGas.textContent = isMQ2Fault ? 'ERR' : valGas.textContent;
  }
  if (planTemp && valTemp) {
    planTemp.textContent = isDHTFault ? 'ERR' : valTemp.textContent;
  }
  if (planHum && valHum) {
    planHum.textContent = isDHTFault ? 'ERR' : valHum.textContent;
  }

  // 2. Cập nhật trạng thái Phòng khách (Trạm chính)
  if (zoneLiving) {
    if (currentFireState) {
      zoneLiving.className = "room-zone danger";
    } else {
      zoneLiving.className = "room-zone safe";
    }
  }

  // 3. Cập nhật trạng thái Phòng ngủ (Trạm phụ)
  if (zoneBed) {
    if (isPreAlarm) {
      zoneBed.className = "room-zone warning";
      if (badgeTempTrend) {
        badgeTempTrend.className = "device-badge active";
        badgeTempTrend.textContent = "Nhiệt tăng nhanh!";
        badgeTempTrend.style.color = "#f97316";
        badgeTempTrend.style.borderColor = "rgba(249,115,22,0.3)";
      }
    } else {
      zoneBed.className = "room-zone safe";
      if (badgeTempTrend) {
        badgeTempTrend.className = "device-badge";
        badgeTempTrend.textContent = "Bình thường";
        badgeTempTrend.style.color = "";
        badgeTempTrend.style.borderColor = "";
      }
    }
  }

  // 4. Cập nhật trạng thái Hành lang (Camera)
  if (zoneCorridor) {
    zoneCorridor.className = cameraIP ? "room-zone safe" : "room-zone";
  }
}

function muteAlarm() {
  db.ref('Lenh_Tat_Bao_Dong').set(1).then(() => {
    addLog('🔕 Đã gửi lệnh TẮT CÒI lên ESP32', 'wrn');
    addNotification('Tắt còi báo động', 'Lệnh đã gửi đến ESP32', 'warning');
  });
}

function toggleFire() {
  const on = document.getElementById('toggle').classList.contains('on');
  db.ref('Trang_Thai_Chay').set(on ? 0 : 1);
}

function togglePump() {
  const on = document.getElementById('togglePump').classList.contains('on');
  db.ref('Relay2').set(on ? 0 : 1).then(() => {
    addLog(`Đã gửi lệnh ${on ? 'TẮT' : 'BẬT'} MÁY BƠM`, 'info');
  });
}

function toggleFan() {
  const on = document.getElementById('toggleFan').classList.contains('on');
  db.ref('Relay1').set(on ? 0 : 1).then(() => {
    addLog(`Đã gửi lệnh ${on ? 'TẮT' : 'BẬT'} QUẠT HÚT`, 'info');
  });
}

function toggleWindow() {
  const on = document.getElementById('toggleWindow').classList.contains('on');
  db.ref('Cua_So').set(on ? 0 : 1).then(() => {
    addLog(`Đã gửi lệnh ${on ? 'ĐÓNG' : 'MỞ'} CỬA SỔ (Servo)`, 'info');
  });
}

function callFireDept() {
  if (confirm('📞 Xác nhận gọi 114 — Cứu Hỏa?')) {
    window.location.href = 'tel:114';
    addLog('📞 Đã gọi cứu hỏa 114', 'err');
    addNotification('Gọi cứu hỏa', 'Đã kết nối đến số 114', 'danger');
  }
}

function saveCameraIP() {
  const input = document.getElementById('inputCamIP');
  if (!input) return;
  const ip = input.value.trim();
  if (!ip) {
    alert('Vui lòng nhập địa chỉ IP hợp lệ.');
    return;
  }
  const ipPattern = /^(\d{1,3}\.){3}\d{1,3}$/;
  if (!ipPattern.test(ip)) {
    alert('Định dạng IP không hợp lệ. Ví dụ: 192.168.1.100');
    return;
  }
  db.ref('Camera_IP').set(ip).then(() => {
    addLog(`Đã cập nhật IP Camera mới: ${ip}`, 'ok');
    addNotification('Cấu hình Camera', 'Đã cập nhật IP Camera thành công', 'success');
    input.value = '';
  }).catch(err => {
    alert('Lỗi lưu IP: ' + err.message);
  });
}
