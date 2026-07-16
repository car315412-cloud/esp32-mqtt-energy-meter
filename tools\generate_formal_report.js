const fs = require("fs");
const path = require("path");
const {
  AlignmentType,
  BorderStyle,
  Document,
  Footer,
  HeadingLevel,
  ImageRun,
  PageBreak,
  PageNumber,
  Packer,
  Paragraph,
  ShadingType,
  Table,
  TableCell,
  TableRow,
  TextRun,
  WidthType,
} = require("./.report_tools/node_modules/docx");

const baseFont = "標楷體";
const workspaceRoot = __dirname;
const imageRoot = path.join(
  workspaceRoot,
  "05_結案報告-20260716T053927Z-1-001",
  "05_結案報告"
);
const outputPath = path.join(workspaceRoot, "結案報告.docx");

function readImageSize(filePath) {
  const buf = fs.readFileSync(filePath);

  if (
    buf.length >= 24 &&
    buf[0] === 0x89 &&
    buf[1] === 0x50 &&
    buf[2] === 0x4e &&
    buf[3] === 0x47
  ) {
    return { width: buf.readUInt32BE(16), height: buf.readUInt32BE(20) };
  }

  if (buf.length >= 4 && buf[0] === 0xff && buf[1] === 0xd8) {
    let offset = 2;
    while (offset < buf.length) {
      if (buf[offset] !== 0xff) {
        offset += 1;
        continue;
      }

      const marker = buf[offset + 1];
      const length = buf.readUInt16BE(offset + 2);
      const isSof =
        (marker >= 0xc0 && marker <= 0xc3) ||
        (marker >= 0xc5 && marker <= 0xc7) ||
        (marker >= 0xc9 && marker <= 0xcb) ||
        (marker >= 0xcd && marker <= 0xcf);

      if (isSof) {
        return {
          width: buf.readUInt16BE(offset + 7),
          height: buf.readUInt16BE(offset + 5),
        };
      }

      offset += 2 + length;
    }
  }

  throw new Error(`Unsupported image format: ${filePath}`);
}

function fitImage(filePath, maxWidth = 660, maxHeight = 440) {
  const { width, height } = readImageSize(filePath);
  const scale = Math.min(maxWidth / width, maxHeight / height, 1);
  return {
    width: Math.max(1, Math.round(width * scale)),
    height: Math.max(1, Math.round(height * scale)),
  };
}

function p(text, options = {}) {
  return new Paragraph({
    alignment: options.alignment ?? AlignmentType.LEFT,
    pageBreakBefore: options.pageBreakBefore ?? false,
    spacing: {
      before: options.before ?? 0,
      after: options.after ?? 120,
      line: options.line ?? 280,
    },
    children: [
      new TextRun({
        text,
        bold: options.bold ?? false,
        italics: options.italics ?? false,
        size: options.size ?? 24,
        font: options.font ?? baseFont,
        color: options.color,
      }),
    ],
  });
}

function heading(text, level = HeadingLevel.HEADING_1, pageBreakBefore = false) {
  return new Paragraph({
    heading: level,
    pageBreakBefore,
    spacing: { before: 180, after: 120 },
    children: [
      new TextRun({
        text,
        bold: true,
        size: level === HeadingLevel.HEADING_1 ? 32 : 26,
        font: baseFont,
        color: "0F5E7A",
      }),
    ],
  });
}

function bullet(text) {
  return new Paragraph({
    bullet: { level: 0 },
    spacing: { line: 260, after: 60 },
    children: [
      new TextRun({
        text,
        size: 22,
        font: baseFont,
      }),
    ],
  });
}

function caption(text, figureNo) {
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 120 },
    children: [
      new TextRun({
        text: `圖 ${figureNo}　${text}`,
        italics: true,
        size: 20,
        font: baseFont,
        color: "666666",
      }),
    ],
  });
}

function figure(fileName, figureNo, captionText, maxWidth = 660, maxHeight = 440) {
  const filePath = path.join(imageRoot, fileName);
  const ext = path.extname(fileName).toLowerCase();
  const imageType = ext === ".jpg" || ext === ".jpeg" ? "jpg" : ext.slice(1);
  const dims = fitImage(filePath, maxWidth, maxHeight);

  return [
    new Table({
      width: { size: 100, type: WidthType.PERCENTAGE },
      borders: {
        top: { style: BorderStyle.SINGLE, size: 1, color: "D9E2EC" },
        bottom: { style: BorderStyle.SINGLE, size: 1, color: "D9E2EC" },
        left: { style: BorderStyle.SINGLE, size: 1, color: "D9E2EC" },
        right: { style: BorderStyle.SINGLE, size: 1, color: "D9E2EC" },
        insideHorizontal: { style: BorderStyle.NONE, size: 0, color: "FFFFFF" },
        insideVertical: { style: BorderStyle.NONE, size: 0, color: "FFFFFF" },
      },
      rows: [
        new TableRow({
          children: [
            new TableCell({
              shading: { type: ShadingType.CLEAR, fill: "F7F9FB", color: "F7F9FB" },
              children: [
                new Paragraph({
                  alignment: AlignmentType.CENTER,
                  spacing: { before: 80, after: 40 },
                  children: [
                    new ImageRun({
                      type: imageType,
                      data: fs.readFileSync(filePath),
                      transformation: dims,
                    }),
                  ],
                }),
                caption(captionText, figureNo),
              ],
            }),
          ],
        }),
      ],
    }),
  ];
}

function coverPhoto(fileName, maxWidth = 430, maxHeight = 520) {
  const filePath = path.join(workspaceRoot, fileName);
  const ext = path.extname(fileName).toLowerCase();
  const imageType = ext === ".jpg" || ext === ".jpeg" ? "jpg" : ext.slice(1);
  const dims = fitImage(filePath, maxWidth, maxHeight);

  return new Table({
    width: { size: 70, type: WidthType.PERCENTAGE },
    alignment: AlignmentType.CENTER,
    borders: {
      top: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      bottom: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      left: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      right: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      insideHorizontal: { style: BorderStyle.NONE, size: 0, color: "FFFFFF" },
      insideVertical: { style: BorderStyle.NONE, size: 0, color: "FFFFFF" },
    },
    rows: [
      new TableRow({
        children: [
          new TableCell({
            shading: { type: ShadingType.CLEAR, fill: "F7F9FB", color: "F7F9FB" },
            children: [
              new Paragraph({
                alignment: AlignmentType.CENTER,
                spacing: { before: 70, after: 50 },
                children: [
                  new ImageRun({
                    type: imageType,
                    data: fs.readFileSync(filePath),
                    transformation: dims,
                  }),
                ],
              }),
            ],
          }),
        ],
      }),
    ],
  });
}

function infoTable(rows) {
  return new Table({
    width: { size: 100, type: WidthType.PERCENTAGE },
    borders: {
      top: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      bottom: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      left: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      right: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      insideHorizontal: { style: BorderStyle.SINGLE, size: 1, color: "D7E1E7" },
      insideVertical: { style: BorderStyle.SINGLE, size: 1, color: "D7E1E7" },
    },
    rows: rows.map(
      ([left, right]) =>
        new TableRow({
          children: [
            new TableCell({
              shading: { type: ShadingType.CLEAR, fill: "EDF4F8", color: "EDF4F8" },
              children: [p(left, { bold: true, alignment: AlignmentType.CENTER, size: 22, after: 60 })],
            }),
            new TableCell({
              children: [p(right, { size: 22, after: 60 })],
            }),
          ],
        })
    ),
  });
}

function tocTable(entries) {
  return new Table({
    width: { size: 100, type: WidthType.PERCENTAGE },
    borders: {
      top: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      bottom: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      left: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      right: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      insideHorizontal: { style: BorderStyle.SINGLE, size: 1, color: "D7E1E7" },
      insideVertical: { style: BorderStyle.SINGLE, size: 1, color: "D7E1E7" },
    },
    rows: entries.map(
      (entry) =>
        new TableRow({
          children: [
            new TableCell({
              width: { size: 84, type: WidthType.PERCENTAGE },
              children: [
                p(entry.title, {
                  size: entry.level === 1 ? 22 : 20,
                  bold: entry.level === 1,
                  after: 40,
                  indent: { left: entry.level === 2 ? 360 : 0 },
                }),
              ],
            }),
            new TableCell({
              width: { size: 16, type: WidthType.PERCENTAGE },
              children: [
                p(entry.page ?? " ", { size: 20, alignment: AlignmentType.CENTER, after: 40 }),
              ],
            }),
          ],
        })
    ),
  });
}

function chapterSection(meta) {
  const out = [];
  out.push(heading(meta.title, HeadingLevel.HEADING_1, meta.pageBreakBefore));
  out.push(p(meta.intro, { size: 22 }));

  meta.subsections.forEach(([sub, txt]) => {
    out.push(heading(sub, HeadingLevel.HEADING_2));
    out.push(p(txt, { size: 22 }));
  });

  out.push(heading("本章重點", HeadingLevel.HEADING_2));
  meta.points.forEach((item) => out.push(bullet(item)));

  if (meta.diagram) {
    out.push(heading("系統架構示意", HeadingLevel.HEADING_2));
    out.push(
      p(
        "┌──────────────┐      ┌──────────────┐      ┌──────────────┐      ┌────────────────┐\n" +
          "│ 感測器 / 影像 │ ───▶ │   ESP32 主控   │ ───▶ │ MQTT / HTTP   │ ───▶ │ Node-RED / APP │\n" +
          "└──────────────┘      └──────────────┘      └──────────────┘      └────────────────┘\n" +
          "         ▲                       │\n" +
          "         │                       ▼\n" +
          "     OLED 顯示             本地除錯與回饋",
        { size: 18, alignment: AlignmentType.CENTER, font: baseFont, after: 180 }
      )
    );
  }

  meta.figures.forEach((fig) => {
    out.push(...figure(fig.file, fig.no, fig.caption, fig.maxWidth, fig.maxHeight));
  });

  return out;
}

const chapters = [
  {
    title: "第一章 PCB 電路板設計",
    page: "4",
    intro:
      "本章說明本專案在硬體整合階段所完成之 PCB 版圖規劃、元件配置與打樣成果。透過單一電路板整合主控、感測、顯示與控制介面，可有效降低原型機接線複雜度，並提升系統穩定性與展示性。",
    subsections: [
      ["1.1 設計目標", "本專案以可展示、可維護與可擴充為主要設計原則，先確認 ESP32、顯示模組、感測模組及控制模組之介面需求，再進行板面配置。"],
      ["1.2 接腳配置與訊號分區", "依照數位輸出、感測輸入與電源供應之特性進行分區，避免訊號線交錯過於密集，並降低後續除錯難度。"],
      ["1.3 電源與擴充介面", "板上保留 3V3、5V 與 GND 等通用端子，同時預留外接模組位置，以支援後續增加更多感測或控制功能。"],
      ["1.4 打樣與成品封裝", "完成 PCB 打樣後，配合固定柱與外殼安裝，使成品具備可攜帶、可展示及可重複測試之特性。"],
    ],
    points: [
      "透過 PCB 取代麵包板雛型，可明顯提升整體穩定度。",
      "模組化版圖規劃有助於後續擴充與維修。",
    ],
    figures: [
      { no: "1-1", file: "742711779_2478076935991962_932870192002837318_n.jpg", caption: "PCB 走線與元件配置圖" },
      { no: "1-2", file: "739809355_2922219664652501_1202045770439854862_n.jpg", caption: "PCB 3D 成品示意圖" },
    ],
  },
  {
    title: "第二章 基礎 ESP32",
    page: "6",
    intro:
      "本章整理 ESP32 基礎功能驗證過程，包含開發環境建立、GPIO 輸出、感測元件讀取及 OLED 顯示等內容。由基礎測試逐步擴充，可建立後續整合應用所需之程式與硬體基礎。",
    subsections: [
      ["2.1 開發環境與燒錄流程", "本專案以 Arduino CLI 進行編譯與燒錄，確認板型、序列埠與核心套件皆可正常運作，確保各階段草稿皆能穩定部署。"],
      ["2.2 GPIO 輸出驗證", "透過單色 LED、紅綠燈與 WS2812 等範例，驗證 ESP32 的數位輸出能力與 PWM、時序控制表現。"],
      ["2.3 感測元件基礎測試", "以 PIR、光敏與 DHT 等元件進行輸入讀值測試，建立感測資料取得與基本判讀能力。"],
      ["2.4 OLED 顯示與除錯", "將感測結果即時顯示於 OLED 面板，使開發者可直接透過畫面觀察數值變化，提升除錯效率。"],
    ],
    points: [
      "以單元測試確認每一個元件與腳位的可用性。",
      "序列埠與 OLED 雙重回饋有助於快速除錯。",
    ],
    figures: [
      { no: "2-1", file: "747536353_1312789540838556_185588493872055155_n.jpg", caption: "ESP32 與 OLED、感測元件之基礎接線" },
      { no: "2-2", file: "740917468_1507017800906781_2728645092806424422_n.jpg", caption: "基礎硬體整合與模組化接線示意" },
    ],
  },
  {
    title: "第三章 網路與 MQTT（手機 APP）",
    page: "8",
    intro:
      "本章說明 ESP32 透過 Wi-Fi 與 MQTT 與外部平台連結之流程，並結合手機 APP 或瀏覽器儀表板進行資料監看。此階段的重點在於建立穩定的資料傳輸管道，並形成可供教學與展示之雙向通訊架構。",
    subsections: [
      ["3.1 Wi-Fi 連線建立", "設定無線網路名稱、密碼與主機名稱後，ESP32 即可加入校園或家用網路，作為雲端傳輸的前置條件。"],
      ["3.2 MQTT 主題與資料格式", "採用 MQTT 作為訊息通道，並以 JSON 格式封裝感測資料，使雲端平台與手機端更容易解析與顯示。"],
      ["3.3 手機 APP 監看介面", "以手機端儀表板呈現即時數值與趨勢圖，提供使用者直觀且便利的監看體驗。"],
      ["3.4 遠端控制與回饋機制", "部分版本已加入控制主題，讓使用者可由手機端發送指令回 ESP32，達成基本遠端操作功能。"],
    ],
    points: [
      "MQTT 可有效降低感測資料與控制訊息的整合成本。",
      "手機端介面使專案成果更具實用性與展示性。",
    ],
    figures: [
      { no: "3-1", file: "4.png", caption: "MQTT Broker 與連線設定畫面", maxWidth: 620, maxHeight: 400 },
      { no: "3-2", file: "3.png", caption: "手機或瀏覽器即時監看儀表板", maxWidth: 620, maxHeight: 400 },
      { no: "3-3", file: "5.png", caption: "資料趨勢與控制元件整合畫面", maxWidth: 620, maxHeight: 400 },
    ],
  },
  {
    title: "第四章 環境監測與公開資訊",
    page: "10",
    intro:
      "本章將本地環境感測與公開資料來源整合於同一系統中，包含溫濕度、亮度、時間同步與空氣品質等資訊。此類整合可讓專案從單純感測延伸至資料應用，進一步提升系統價值。",
    subsections: [
      ["4.1 環境資料擷取", "透過光敏元件與 DHT 感測器讀取現場資料，並轉換為更易理解的百分比或數值資訊。"],
      ["4.2 NTP 校時", "導入網路校時機制，使裝置畫面與紀錄時間一致，便於進行觀察與查驗。"],
      ["4.3 公開資訊整合", "串接政府或公共資料來源，使專案具備外部資訊查詢與展示能力，增加實際應用情境。"],
      ["4.4 多頁顯示設計", "透過分頁式畫面安排不同資訊類別，使 OLED 呈現更有條理且易於閱讀。"],
    ],
    points: [
      "環境監測與公開資料可整合於同一顯示介面。",
      "資料來源多元化後，系統展示內容更完整。",
    ],
    figures: [
      { no: "4-1", file: "WIN_20260716_13_28_54_Pro.jpg", caption: "環境監測實機顯示畫面" },
      { no: "4-2", file: "2.png", caption: "公開資訊與交通影像資料整合頁面", maxWidth: 620, maxHeight: 400 },
      { no: "4-3", file: "747941411_1286448426694019_3807202194689559046_n.jpg", caption: "Node-RED 公開資訊流程與影像預覽", maxWidth: 620, maxHeight: 400 },
    ],
  },
  {
    title: "第五章 即時影像",
    page: "12",
    intro:
      "本章整理即時影像擷取與雲端傳輸流程。專案透過影像模組進行畫面擷取，再經由資料編碼與訊息傳輸送至後端平台，讓使用者可即時觀看最新影像內容。",
    subsections: [
      ["5.1 影像擷取流程", "由攝影模組取得當前影像後，送入後端處理程序，以形成可傳輸之資料格式。"],
      ["5.2 編碼與傳輸方式", "以 JPEG 壓縮與 Base64 編碼方式提高傳輸相容性，並透過既有通訊通道完成上傳。"],
      ["5.3 預覽與展示", "後端以 image preview 或相似元件呈現最新畫面，便於進行即時監看。"],
      ["5.4 調整與修正", "實作期間曾針對影像方向、頻率與顯示畫面進行調整，最終完成穩定的即時影像版本。"],
    ],
    points: [
      "即時影像擴充了 ESP32 專案的視覺化監控能力。",
      "影像與感測資料並存，有助於建構完整情境。",
    ],
    figures: [
      { no: "5-1", file: "747168672_1279264934093589_9002253920497367325_n.jpg", caption: "即時影像預覽與交通畫面回傳" },
      { no: "5-2", file: "747941411_1286448426694019_3807202194689559046_n.jpg", caption: "影像流與公開資訊流程的整合方式", maxWidth: 620, maxHeight: 400 },
    ],
  },
  {
    title: "第六章 智慧電表",
    page: "13",
    intro:
      "本章說明智慧電表的量測與控制成果，包含電壓、電流、功率、累積度數與碳排等數據，並搭配繼電器與 SG90 伺服馬達進行控制，形成完整的智慧能源監測原型。",
    subsections: [
      ["6.1 電力參數量測", "系統可即時讀取電壓、電流、功率與度數等資訊，讓能耗狀態具有明確的數據依據。"],
      ["6.2 繼電器控制", "透過控制模組切換負載狀態，展示遠端開關與能源管理的實作可能。"],
      ["6.3 SG90 伺服控制", "以角度控制方式模擬機構動作，作為智慧控制延伸應用之一。"],
      ["6.4 顯示與同步", "量測結果同時回饋至 OLED 與儀表板，兼顧現場可視化與雲端監看。"],
    ],
    points: [
      "智慧電表是本專案最具整合性的應用成果之一。",
      "實機畫面能有效呈現感測與控制的結合效果。",
    ],
    figures: [
      { no: "6-1", file: "WIN_20260716_13_29_21_Pro.jpg", caption: "智慧電表實機與顯示模組整合" },
      { no: "6-2", file: "744738689_1030193686405715_8846608852409559452_n.jpg", caption: "手機端智慧電表儀表板" },
    ],
  },
  {
    title: "第七章 Node-RED 流程與儀表板",
    page: "14",
    intro:
      "本章彙整 Node-RED 的流程規劃、資料串接與儀表板排版設計。透過流程化節點將感測、影像與控制資料整合，可形成適合教學與展示之視覺化中控介面。",
    subsections: [
      ["7.1 流程節點規劃", "依資料接收、格式轉換、視覺呈現與控制輸出等功能進行分層設計，以提高維護性。"],
      ["7.2 儀表板分頁", "以公開資訊、能源資訊與影像預覽等分頁區分資料類型，使畫面更有系統性。"],
      ["7.3 圖表與儀表元件", "運用 gauge、chart、template、image preview 等元件，呈現即時數值與趨勢圖。"],
      ["7.4 整合展示", "將多來源資料集中於同一平台，可作為專題展示、課堂示範與後續延伸之基礎。"],
    ],
    points: [
      "Node-RED 是整體專案的視覺化整合中樞。",
      "以圖表與儀表板呈現資料，可大幅提升成果可讀性。",
    ],
    figures: [
      { no: "7-1", file: "未命名.png", caption: "能源資訊分頁與儀表板總覽", maxWidth: 620, maxHeight: 400 },
      { no: "7-2", file: "3.png", caption: "公開資訊儀表板卡片式排版", maxWidth: 620, maxHeight: 400 },
      { no: "7-3", file: "5.png", caption: "監看與控制元件配置", maxWidth: 620, maxHeight: 400 },
    ],
    diagram: true,
    pageBreakBefore: true,
  },
];

const tocEntries = [
  { title: "前言", page: "1", level: 1 },
  { title: "摘要", page: "1", level: 2 },
  { title: "研究動機", page: "2", level: 2 },
  { title: "研究目的", page: "3", level: 2 },
  ...chapters.map((c) => ({ title: c.title, page: c.page, level: 1 })),
  { title: "結語與後續展望", page: "16", level: 1 },
];

const children = [];

children.push(
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 180, before: 20 },
    children: [
      new TextRun({
        text: "ESP32 與 AI 寫程式",
        bold: true,
        size: 34,
        font: baseFont,
        color: "0F5E7A",
      }),
    ],
  })
);
children.push(
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 110 },
    children: [
      new TextRun({
        text: "結案報告",
        bold: true,
        size: 44,
        font: baseFont,
        color: "1F1F1F",
      }),
    ],
  })
);
children.push(
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 180 },
    children: [
      new TextRun({
        text: "以 ESP32 為核心之感測、通訊、顯示與雲端整合成果",
        size: 22,
        font: baseFont,
        color: "4D4D4D",
      }),
    ],
  })
);

children.push(
  new Table({
    width: { size: 100, type: WidthType.PERCENTAGE },
    borders: {
      top: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      bottom: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      left: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      right: { style: BorderStyle.SINGLE, size: 1, color: "B8C7D1" },
      insideHorizontal: { style: BorderStyle.SINGLE, size: 1, color: "D7E1E7" },
      insideVertical: { style: BorderStyle.SINGLE, size: 1, color: "D7E1E7" },
    },
    rows: [
      ["專題名稱", "ESP32 與 AI 寫程式 結案報告"],
      ["作者", "傅貴煒"],
      ["日期", "2026 年 7 月 16 日"],
      ["報告範圍", "PCB、基礎 ESP32、MQTT、環境監測、即時影像、智慧電表、Node-RED"],
    ].map(
      ([left, right]) =>
        new TableRow({
          children: [
            new TableCell({
              shading: { type: ShadingType.CLEAR, fill: "EDF4F8", color: "EDF4F8" },
              children: [p(left, { bold: true, alignment: AlignmentType.CENTER, size: 22, after: 60 })],
            }),
            new TableCell({
              children: [p(right, { size: 22, after: 60 })],
            }),
          ],
        })
    ),
  })
);

children.push(
  p("本報告依據蒐集之實作圖片與程式成果整理而成，內容涵蓋硬體設計、基礎測試、雲端應用與展示介面，並以正式專題報告格式呈現。", {
    alignment: AlignmentType.CENTER,
    size: 20,
    after: 40,
  })
);
children.push(coverPhoto("成果相片.png"));
children.push(
  p("成果相片", {
    alignment: AlignmentType.CENTER,
    size: 18,
    italics: true,
    after: 80,
  })
);
children.push(
  new Paragraph({
    border: {
      bottom: { style: BorderStyle.SINGLE, size: 6, color: "0F5E7A" },
    },
    spacing: { after: 140 },
  })
);

children.push(new Paragraph({ children: [new PageBreak()] }));

children.push(heading("目錄", HeadingLevel.HEADING_1));
children.push(tocTable(tocEntries));
children.push(new Paragraph({ children: [new PageBreak()] }));

children.push(heading("前言", HeadingLevel.HEADING_1));
children.push(heading("摘要", HeadingLevel.HEADING_2));
children.push(
  p(
    "本專題以 ESP32 為核心控制平台，從基礎 I/O 測試開始，逐步整合 OLED 顯示、感測資料蒐集、MQTT 通訊、即時影像、智慧電表與 Node-RED 儀表板等功能，完成一套具備教學性與展示性的物聯網整合系統。整體開發過程除驗證硬體模組之可行性外，也同步建立雲端資料傳輸與視覺化呈現機制，使專題成果具備完整度與延伸價值。",
    { size: 22 }
  )
);
children.push(heading("研究動機", HeadingLevel.HEADING_2));
children.push(
  p(
    "在物聯網應用逐漸普及的背景下，感測資料的取得、傳輸、顯示與控制已成為重要的系統能力。為了讓學習者能同時理解硬體接線、程式控制、雲端通訊與資料視覺化，本專題以 ESP32 作為主軸，結合多種感測與控制模組，將單一功能的練習整合成可實際運作的系統。透過此一主題，不僅能強化程式設計與電路實作能力，也能累積後續進行智慧監控與自動化應用的基礎。",
    { size: 22 }
  )
);
children.push(heading("研究目的", HeadingLevel.HEADING_2));
children.push(
  p(
    "本專題之目的在於建立一套以 ESP32 為核心的整合型應用架構，並完成以下目標：一、驗證 ESP32 基本 GPIO 與感測顯示功能；二、建置 MQTT 與手機 APP 的資料傳輸機制；三、整合環境監測與公開資訊顯示；四、實作即時影像與智慧電表應用；五、以 Node-RED 建立可視化儀表板，作為系統整合與成果展示平台。",
    { size: 22 }
  )
);

children.push(new Paragraph({ children: [new PageBreak()] }));

chapters.forEach((chapter) => {
  children.push(...chapterSection(chapter));
});

children.push(heading("結語與後續展望", HeadingLevel.HEADING_1, true));
children.push(
  p(
    "本專題從基礎 I/O 測試逐步延伸至網路通訊、環境監測、即時影像與智慧電表，並最終以 Node-RED 與手機端儀表板完成整體整合。整體成果顯示，ESP32 不僅適合作為教學平台，也能作為物聯網原型開發的核心控制器。",
    { size: 22 }
  )
);
children.push(
  p(
    "後續若持續擴充，建議可加入資料庫儲存、異常警示、權限管理、更多感測器與更完整的外殼設計，讓系統由展示型原型進一步走向實務應用。",
    { size: 22 }
  )
);

const doc = new Document({
  styles: {
    default: {
      document: { run: { font: baseFont } },
      heading1: { run: { font: baseFont } },
      heading2: { run: { font: baseFont } },
    },
  },
  features: {
    updateFields: true,
  },
  sections: [
    {
      properties: {
        titlePage: true,
        page: {
          margin: {
            top: 1440,
            right: 1440,
            bottom: 1440,
            left: 1440,
          },
          pageNumbers: { start: 1 },
        },
      },
      footers: {
        default: new Footer({
          children: [
            new Paragraph({
              alignment: AlignmentType.CENTER,
              children: [
                new TextRun({ text: "第 ", size: 18, font: baseFont }),
                PageNumber.CURRENT,
                new TextRun({ text: " 頁", size: 18, font: baseFont }),
              ],
            }),
          ],
        }),
      },
      children,
    },
  ],
});

Packer.toBuffer(doc).then((buffer) => {
  fs.writeFileSync(outputPath, buffer);
  console.log(`Wrote ${outputPath}`);
});
