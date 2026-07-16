const chapterData = [
  {
    index: "01",
    title: "PCB 電路板設計",
    summary: "從元件配置、配線規劃到實體接線，建立可複用的硬體基礎。",
    bullets: [
      "PCB 佈局與模組定位",
      "電源與訊號線路規劃",
      "焊接、接線與初步測試",
    ],
    tags: ["PCB", "硬體", "接線"],
  },
  {
    index: "02",
    title: "基礎 ESP32",
    summary: "先掌握 GPIO、PWM、ADC 與 OLED 顯示，再進一步整合周邊模組。",
    bullets: [
      "GPIO 控制與輸入輸出觀念",
      "OLED 顯示與基本除錯",
      "Arduino IDE / ESP32 Core 開發流程",
    ],
    tags: ["GPIO", "OLED", "入門"],
  },
  {
    index: "03",
    title: "網路與 MQTT（手機 APP）",
    summary: "讓 ESP32 透過 Wi-Fi 與 MQTT 與手機 APP 雙向溝通。",
    bullets: [
      "Wi-Fi 連線與 MQTT 主題設計",
      "手機 APP / 遠端控制畫面",
      "資料發布、訂閱與即時回應",
    ],
    tags: ["Wi-Fi", "MQTT", "APP"],
  },
  {
    index: "04",
    title: "環境監測與公開資訊",
    summary: "結合感測器、NTP 與公開 API，讓裝置不只會量測，也會理解環境。",
    bullets: [
      "DHT、PIR 與環境狀態顯示",
      "NTP 校時與時間同步",
      "公開資訊 API 串接與顯示",
    ],
    tags: ["感測", "API", "NTP"],
  },
  {
    index: "05",
    title: "即時影像",
    summary: "使用 ESP32-CAM 建立可即時觀看的影像功能，提升展示效果。",
    bullets: [
      "ESP32-CAM 影像輸出",
      "HTTP 影像頁與相機腳位設定",
      "即時畫面與 MQTT 整合",
    ],
    tags: ["ESP32-CAM", "影像", "HTTP"],
  },
  {
    index: "06",
    title: "智慧電表",
    summary: "以能源量測為主軸，呈現電壓、電流、功率與控制功能。",
    bullets: [
      "PZEM 能源量測與資料讀取",
      "繼電器、伺服馬達與控制邏輯",
      "功耗統計與狀態顯示",
    ],
    tags: ["PZEM", "Relay", "能源"],
  },
  {
    index: "07",
    title: "Node-RED 流程與儀表板",
    summary: "把資料轉化成流程與視覺化介面，形成可展示的完整系統。",
    bullets: [
      "Node-RED 流程設計",
      "Dashboard 儀表板與圖表",
      "自動化、告警與資料整合",
    ],
    tags: ["Dashboard", "流程", "可視化"],
  },
];

const chapterGrid = document.getElementById("chapterGrid");

chapterGrid.innerHTML = chapterData.map((chapter) => `
  <article class="chapter-card reveal">
    <div class="chapter-head">
      <div class="chapter-index">${chapter.index}</div>
      <h3>${chapter.title}</h3>
    </div>
    <p class="chapter-copy">${chapter.summary}</p>
    <ul class="chapter-list">
      ${chapter.bullets.map((item) => `<li>${item}</li>`).join("")}
    </ul>
    <div class="chapter-footer">
      ${chapter.tags.map((tag) => `<span class="tag">${tag}</span>`).join("")}
    </div>
  </article>
`).join("");

const revealItems = document.querySelectorAll(".reveal");
const observer = new IntersectionObserver(
  (entries) => {
    entries.forEach((entry) => {
      if (entry.isIntersecting) {
        entry.target.classList.add("in-view");
        observer.unobserve(entry.target);
      }
    });
  },
  { threshold: 0.12 }
);

revealItems.forEach((node, index) => {
  node.style.transitionDelay = `${Math.min(index * 60, 360)}ms`;
  observer.observe(node);
});

document.querySelectorAll('a[href^="#"]').forEach((anchor) => {
  anchor.addEventListener("click", (event) => {
    const targetId = anchor.getAttribute("href");
    if (!targetId || targetId === "#") return;
    const target = document.querySelector(targetId);
    if (!target) return;
    event.preventDefault();
    target.scrollIntoView({ behavior: "smooth", block: "start" });
  });
});
