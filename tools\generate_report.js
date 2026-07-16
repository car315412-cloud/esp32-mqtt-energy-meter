const fs = require("fs");
const path = require("path");
const {
  AlignmentType,
  BorderStyle,
  Document,
  HeadingLevel,
  ImageRun,
  PageBreak,
  Packer,
  Paragraph,
  Table,
  TableCell,
  TableRow,
  TextRun,
  WidthType,
} = require("./.report_tools/node_modules/docx");

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
    return {
      width: buf.readUInt32BE(16),
      height: buf.readUInt32BE(20),
    };
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
        marker >= 0xc0 &&
        marker <= 0xc3 ||
        marker >= 0xc5 &&
        marker <= 0xc7 ||
        marker >= 0xc9 &&
        marker <= 0xcb ||
        marker >= 0xcd &&
        marker <= 0xcf;

      if (isSof) {
        return {
          height: buf.readUInt16BE(offset + 5),
          width: buf.readUInt16BE(offset + 7),
        };
      }

      offset += 2 + length;
    }
  }

  throw new Error(`Unsupported image format: ${filePath}`);
}

function fitImage(filePath, maxWidth = 680, maxHeight = 440) {
  const { width, height } = readImageSize(filePath);
  const scale = Math.min(maxWidth / width, maxHeight / height, 1);
  return {
    width: Math.max(1, Math.round(width * scale)),
    height: Math.max(1, Math.round(height * scale)),
  };
}

function makeParagraph(text, options = {}) {
  return new Paragraph({
    spacing: { line: 280, after: options.after ?? 120 },
    alignment: options.alignment ?? AlignmentType.LEFT,
    pageBreakBefore: options.pageBreakBefore ?? false,
    children: [
      new TextRun({
        text,
        bold: options.bold ?? false,
        italics: options.italics ?? false,
        size: options.size ?? 24,
        color: options.color,
        font: options.font ?? "Microsoft JhengHei",
      }),
    ],
  });
}

function makeBullet(text) {
  return new Paragraph({
    bullet: { level: 0 },
    spacing: { line: 260, after: 60 },
    children: [
      new TextRun({
        text,
        size: 22,
        font: "Microsoft JhengHei",
      }),
    ],
  });
}

function makeHeading(text, level = HeadingLevel.HEADING_1, pageBreakBefore = false) {
  return new Paragraph({
    heading: level,
    pageBreakBefore,
    spacing: { before: 160, after: 120 },
    children: [
      new TextRun({
        text,
        bold: true,
        size: level === HeadingLevel.HEADING_1 ? 32 : 26,
        font: "Microsoft JhengHei",
        color: "0F5E7A",
      }),
    ],
  });
}

function makeCaption(text) {
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 140 },
    children: [
      new TextRun({
        text,
        italics: true,
        size: 20,
        font: "Microsoft JhengHei",
        color: "666666",
      }),
    ],
  });
}

function addImage(children, fileName, caption, maxWidth, maxHeight) {
  const filePath = path.join(imageRoot, fileName);
  const ext = path.extname(fileName).toLowerCase();
  const imageType = ext === ".jpg" || ext === ".jpeg" ? "jpg" : ext.slice(1);
  const dims = fitImage(filePath, maxWidth, maxHeight);
  children.push(
    new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { after: 60 },
      children: [
        new ImageRun({
          type: imageType,
          data: fs.readFileSync(filePath),
          transformation: dims,
        }),
      ],
    })
  );
  children.push(makeCaption(caption));
}

function makeOutlineTable(rows) {
  const header = new TableRow({
    children: [
      new TableCell({
        children: [makeParagraph("章節", { bold: true, alignment: AlignmentType.CENTER, size: 22 })],
      }),
      new TableCell({
        children: [makeParagraph("小節重點", { bold: true, alignment: AlignmentType.CENTER, size: 22 })],
      }),
    ],
  });

  const body = rows.map((row) => new TableRow({
    children: [
      new TableCell({ children: [makeParagraph(row.chapter, { size: 22 })] }),
      new TableCell({ children: [makeParagraph(row.points, { size: 22 })] }),
    ],
  }));

  return new Table({
    width: {
      size: 100,
      type: WidthType.PERCENTAGE,
    },
    borders: {
      top: { style: BorderStyle.SINGLE, size: 1, color: "BBBBBB" },
      bottom: { style: BorderStyle.SINGLE, size: 1, color: "BBBBBB" },
      left: { style: BorderStyle.SINGLE, size: 1, color: "BBBBBB" },
      right: { style: BorderStyle.SINGLE, size: 1, color: "BBBBBB" },
      insideHorizontal: { style: BorderStyle.SINGLE, size: 1, color: "DDDDDD" },
      insideVertical: { style: BorderStyle.SINGLE, size: 1, color: "DDDDDD" },
    },
    rows: [header, ...body],
  });
}

const chapters = [
  {
    title: "第一章 PCB 電路板設計",
    intro:
      "本章整理專案硬體的版圖規劃與 PCB 打樣成果，重點在於把 ESP32、感測器、顯示模組、控制元件與電源介面整合到同一塊板子上，降低接線複雜度並提升可攜性。",
    subsections: [
      ["1.1 設計目標", "先確立主控板需支援的功能，包括感測、顯示、通訊與輸出控制，並保留教學與後續擴充空間。"],
      ["1.2 接腳與元件配置", "依照訊號性質區分感測區、控制區與電源區，讓佈線更清楚，也降低雜訊互相干擾的風險。"],
      ["1.3 供電與擴充", "規劃 3V3、5V 與 GND 介面，方便不同模組共用；同時預留排針與外接端子，便於後續功能加掛。"],
      ["1.4 打樣與封裝", "透過 PCB 打樣、固定柱與外殼配置，讓成品從麵包板雛型逐步走向可展示的完整系統。"],
    ],
    bullets: [
      "以模組化配置縮短接線時間。",
      "透過 PCB 打樣提升成品穩定度與展示性。",
    ],
    images: [
      { file: "742711779_2478076935991962_932870192002837318_n.jpg", caption: "圖 1-1 PCB 走線與元件配置圖" },
      { file: "739809355_2922219664652501_1202045770439854862_n.jpg", caption: "圖 1-2 PCB 3D 成品示意圖" },
    ],
  },
  {
    title: "第二章 基礎 ESP32",
    intro:
      "本章說明 ESP32 的基礎測試流程，從最小單元的 GPIO 驗證開始，逐步加入 LED、RGB、PIR、OLED 等元件，建立後續整合開發所需的硬體與程式基礎。",
    subsections: [
      ["2.1 開發環境與燒錄流程", "確認 Arduino CLI、板型、序列埠與上傳流程，使每個草稿都能穩定編譯與燒錄。"],
      ["2.2 GPIO 基本輸出", "以單色 LED、紅綠燈與 WS2812 驗證輸出腳位與時序控制，熟悉 ESP32 的 GPIO 行為。"],
      ["2.3 基本輸入感測", "利用 PIR 與光敏元件檢查輸入讀值與門檻判斷，作為後續環境監測的起點。"],
      ["2.4 OLED 顯示與除錯", "將感測值即時寫入 OLED，讓開發者可直接從面板判斷資料是否正常進來。"],
    ],
    bullets: [
      "先完成單一功能，再做多模組整合。",
      "以序列埠與 OLED 雙重回饋提升除錯效率。",
    ],
    images: [
      { file: "747536353_1312789540838556_185588493872055155_n.jpg", caption: "圖 2-1 ESP32 與 OLED / 感測元件的基礎接線" },
      { file: "740917468_1507017800906781_2728645092806424422_n.jpg", caption: "圖 2-2 基礎硬體整合與模組化接線示意" },
    ],
  },
  {
    title: "第三章 網路與 MQTT（手機 APP）",
    intro:
      "本章聚焦 Wi-Fi 連線、MQTT 主題設計與手機端監看介面的整合。透過手機 APP 或雲端儀表板，即可看到 ESP32 上傳的感測值，並在需要時進行遠端控制。",
    subsections: [
      ["3.1 Wi-Fi 連線建立", "設定 SSID、密碼與裝置名稱，讓 ESP32 能穩定加入校園或家用無線網路。"],
      ["3.2 MQTT 主題與資料格式", "採用 JSON 作為資料格式，讓溫度、濕度與亮度等資訊都能被清楚辨識與後端解析。"],
      ["3.3 手機 APP 監看", "以手機端儀表板作為即時顯示介面，讓數值不只存在序列埠，而是能直接在手機上查看。"],
      ["3.4 控制與回饋", "部分版本加入控制主題，讓 APP 能回傳指令，達到雙向通訊與遠端操作。"],
    ],
    bullets: [
      "MQTT 讓資料上傳與控制訊息分流更清楚。",
      "手機端視覺化介面可提升展示與教學效果。",
    ],
    images: [
      { file: "4.png", caption: "圖 3-1 MQTT Broker 連線設定頁面" },
      { file: "3.png", caption: "圖 3-2 手機或瀏覽器儀表板即時監看畫面" },
      { file: "5.png", caption: "圖 3-3 資料趨勢與控制元件整合畫面" },
    ],
  },
  {
    title: "第四章 環境監測與公開資訊",
    intro:
      "本章整合光敏、DHT 與時間資訊，並串接公開資料來源，例如 NTP 與空氣品質資料，讓裝置不只是讀取現場感測值，也能同時顯示外部即時資訊。",
    subsections: [
      ["4.1 環境感測資料擷取", "讀取溫度、濕度與亮度，並進行平均或百分比換算，讓資料更容易對照實際環境。"],
      ["4.2 NTP 校時", "透過網路時間協定同步時間，讓裝置畫面與記錄時間一致，方便後續查驗。"],
      ["4.3 公開資訊來源", "加入 AQI 或交通等公開資料，讓專案從單純感測延伸成資訊整合應用。"],
      ["4.4 顯示版面設計", "在 OLED 上以分頁方式呈現多組資訊，兼顧可讀性與資訊量。"],
    ],
    bullets: [
      "把本地感測與外部公開資料放在同一個頁面。",
      "適合做成教學案例，示範資料收集與視覺化流程。",
    ],
    images: [
      { file: "WIN_20260716_13_28_54_Pro.jpg", caption: "圖 4-1 環境監測實機顯示畫面" },
      { file: "2.png", caption: "圖 4-2 公開資訊與交通影像資料整合頁面" },
      { file: "747941411_1286448426694019_3807202194689559046_n.jpg", caption: "圖 4-3 Node-RED 公開資訊流程與影像預覽" },
    ],
  },
  {
    title: "第五章 即時影像",
    intro:
      "本章整理即時影像擷取與上傳流程，包含 ESP32-CAM 影像擷取、JPEG 壓縮、Base64 傳送與 Node-RED 預覽。重點是讓畫面能快速回到雲端介面，形成即時監看能力。",
    subsections: [
      ["5.1 影像擷取流程", "透過攝影模組抓取畫面，再送往後端做資料轉換與顯示。"],
      ["5.2 Base64 與 MQTT 傳輸", "將 JPEG 影像轉成 Base64 後透過 MQTT 上傳，方便在既有訊息管道中快速整合。"],
      ["5.3 顯示與回饋", "在儀表板中以 image preview 呈現影像，讓使用者能直接看到最新畫面。"],
      ["5.4 異常排除", "影像方向、OLED 顯示與上傳頻率都曾進行修正，讓即時影像更穩定。"],
    ],
    bullets: [
      "即時影像是本專案由感測走向視覺化監控的重要一步。",
      "可延伸到遠端巡檢、教室監看或安全應用。",
    ],
    images: [
      { file: "747168672_1279264934093589_9002253920497367325_n.jpg", caption: "圖 5-1 即時影像預覽與交通畫面回傳" },
      { file: "747941411_1286448426694019_3807202194689559046_n.jpg", caption: "圖 5-2 影像流與公開資訊流程的整合方式" },
    ],
  },
  {
    title: "第六章 智慧電表",
    intro:
      "本章說明智慧電表模組的量測與控制成果，包含電壓、電流、功率、累積度數與碳排等資訊，並整合繼電器與 SG90 伺服馬達，形成可控制的電力監測系統。",
    subsections: [
      ["6.1 電力參數量測", "讀取電壓、電流、功率與度數，讓電力消耗有具體數字可供記錄。"],
      ["6.2 繼電器控制", "透過 MQTT 或本機按鈕切換負載，展示智慧開關與遠端控制的可行性。"],
      ["6.3 SG90 伺服控制", "以角度控制方式模擬閥門或機構動作，擴充智慧電表的應用場景。"],
      ["6.4 顯示與雲端同步", "將量測結果同步顯示在 OLED 與儀表板，讓現場與遠端都能即時查看。"],
    ],
    bullets: [
      "硬體實測畫面最能呈現智慧電表的應用價值。",
      "儀表板與實體顯示可互相對照，提高資料可信度。",
    ],
    images: [
      { file: "WIN_20260716_13_29_21_Pro.jpg", caption: "圖 6-1 智慧電表實機與顯示模組整合" },
      { file: "744738689_1030193686405715_8846608852409559452_n.jpg", caption: "圖 6-2 手機端智慧電表儀表板" },
    ],
  },
  {
    title: "第七章 Node-RED 流程與儀表板",
    intro:
      "本章彙整 Node-RED 的流程設計與儀表板排版。透過節點串接、分頁分類、圖表與儀表元件，可以把 MQTT、影像、環境與電力資料整合成可讀性高的控制介面。",
    subsections: [
      ["7.1 流程節點規劃", "將資料接收、格式處理、圖表顯示與控制節點分層管理，方便後續維護。"],
      ["7.2 儀表板分頁", "以公開資訊、能源資訊與影像預覽等分頁區分不同主題，降低畫面雜訊。"],
      ["7.3 圖表與儀表元件", "善用 gauge、chart、template、image preview 等元件，呈現即時與歷史趨勢。"],
      ["7.4 整合與展示", "讓教學、實作與成果發表都能在同一個介面完成，強化專案的整體性。"],
    ],
    bullets: [
      "Node-RED 是整個專案的視覺化中控台。",
      "流程可支援手機、瀏覽器與多種資料來源。",
    ],
    images: [
      { file: "未命名.png", caption: "圖 7-1 能源資訊分頁與儀表板總覽" },
      { file: "3.png", caption: "圖 7-2 公開資訊儀表板卡片式排版" },
      { file: "5.png", caption: "圖 7-3 監看與控制元件配置" },
    ],
    diagram: true,
  },
];

const docChildren = [];

docChildren.push(
  makeParagraph("ESP32 與 AI 寫程式", {
    alignment: AlignmentType.CENTER,
    bold: true,
    size: 36,
    color: "0F5E7A",
    after: 60,
  })
);
docChildren.push(
  makeParagraph("結案報告", {
    alignment: AlignmentType.CENTER,
    bold: true,
    size: 32,
    color: "1F1F1F",
    after: 40,
  })
);
docChildren.push(
  makeParagraph("依據收集圖片整理的七章式成果報告", {
    alignment: AlignmentType.CENTER,
    size: 24,
    color: "555555",
    after: 260,
  })
);

docChildren.push(
  new Table({
    width: {
      size: 100,
      type: WidthType.PERCENTAGE,
    },
    borders: {
      top: { style: BorderStyle.SINGLE, size: 1, color: "BBBBBB" },
      bottom: { style: BorderStyle.SINGLE, size: 1, color: "BBBBBB" },
      left: { style: BorderStyle.SINGLE, size: 1, color: "BBBBBB" },
      right: { style: BorderStyle.SINGLE, size: 1, color: "BBBBBB" },
      insideHorizontal: { style: BorderStyle.SINGLE, size: 1, color: "DDDDDD" },
      insideVertical: { style: BorderStyle.SINGLE, size: 1, color: "DDDDDD" },
    },
    rows: [
      new TableRow({
        children: [
          new TableCell({ children: [makeParagraph("專案範圍", { bold: true, alignment: AlignmentType.CENTER, size: 22 })] }),
          new TableCell({
            children: [makeParagraph("PCB、ESP32 基礎、MQTT 手機 APP、環境監測、即時影像、智慧電表、Node-RED 儀表板", { size: 22 })],
          }),
        ],
      }),
      new TableRow({
        children: [
          new TableCell({ children: [makeParagraph("核心硬體", { bold: true, alignment: AlignmentType.CENTER, size: 22 })] }),
          new TableCell({
            children: [makeParagraph("ESP32 / ESP32-CAM、OLED、DHT、光敏元件、繼電器、SG90、智慧電表模組", { size: 22 })],
          }),
        ],
      }),
      new TableRow({
        children: [
          new TableCell({ children: [makeParagraph("核心軟體", { bold: true, alignment: AlignmentType.CENTER, size: 22 })] }),
          new TableCell({
            children: [makeParagraph("Arduino CLI、MQTT、Node-RED、手機儀表板、公開資料串接", { size: 22 })],
          }),
        ],
      }),
      new TableRow({
        children: [
          new TableCell({ children: [makeParagraph("報告特色", { bold: true, alignment: AlignmentType.CENTER, size: 22 })] }),
          new TableCell({
            children: [makeParagraph("以圖片佐證實機成果，並用流程圖與架構圖整理系統關係。", { size: 22 })],
          }),
        ],
      }),
    ],
  })
);

docChildren.push(makeHeading("章節總覽", HeadingLevel.HEADING_1));
docChildren.push(
  makeOutlineTable(
    chapters.map((chapter) => ({
      chapter: chapter.title,
      points: chapter.subsections.map((item) => item[0]).join("、"),
    }))
  )
);
docChildren.push(new Paragraph({ children: [new PageBreak()] }));

chapters.forEach((chapter, index) => {
  docChildren.push(
    makeHeading(chapter.title, HeadingLevel.HEADING_1, index > 0)
  );
  docChildren.push(makeParagraph(chapter.intro, { size: 22 }));

  chapter.subsections.forEach(([subtitle, text]) => {
    docChildren.push(makeHeading(subtitle, HeadingLevel.HEADING_2));
    docChildren.push(makeParagraph(text, { size: 22 }));
  });

  docChildren.push(makeHeading("本章重點", HeadingLevel.HEADING_2));
  chapter.bullets.forEach((item) => docChildren.push(makeBullet(item)));

  if (chapter.diagram) {
    docChildren.push(makeHeading("系統架構示意", HeadingLevel.HEADING_2));
    docChildren.push(
      new Paragraph({
        alignment: AlignmentType.CENTER,
        spacing: { after: 100 },
        children: [
          new TextRun({
            text:
              "┌──────────────┐     ┌──────────────┐     ┌──────────────┐     ┌────────────────┐\n" +
              "│ 感測器 / 影像 │ ──▶ │   ESP32 主控  │ ──▶ │ MQTT Broker  │ ──▶ │ Node-RED 儀表板 │\n" +
              "└──────────────┘     └──────────────┘     └──────────────┘     └────────────────┘\n" +
              "         ▲                        │\n" +
              "         │                        ▼\n" +
              "     OLED 顯示                 手機 APP / Web 端",
            font: "Consolas",
            size: 18,
          }),
        ],
      })
    );
  }

  chapter.images.forEach((item, imageIndex) => {
    addImage(
      docChildren,
      item.file,
      item.caption,
      imageIndex === 0 && chapter.title.includes("第五章") ? 640 : 680,
      460
    );
  });
});

docChildren.push(makeHeading("結語與後續展望", HeadingLevel.HEADING_1, true));
docChildren.push(
  makeParagraph(
    "本專案從基礎 I/O 測試一路延伸到網路通訊、環境監測、即時影像與智慧電表，最後再由 Node-RED 與手機端儀表板統整成果。整體來看，這不只是單一功能的展示，而是一套可持續擴充的 ESP32 物聯網學習與應用架構。",
    { size: 22 }
  )
);
docChildren.push(
  makeParagraph(
    "後續若要延伸，可再加入資料庫存檔、告警通知、權限管理、更多感測器類型，以及更完整的外殼與工業設計，讓系統從教學原型進一步走向實務場域。",
    { size: 22 }
  )
);

const doc = new Document({
  styles: {
    default: {
      heading1: {
        run: {
          font: "Microsoft JhengHei",
        },
      },
      heading2: {
        run: {
          font: "Microsoft JhengHei",
        },
      },
      document: {
        run: {
          font: "Microsoft JhengHei",
        },
      },
    },
  },
  sections: [
    {
      properties: {
        page: {
          margin: {
            top: 1440,
            right: 1440,
            bottom: 1440,
            left: 1440,
          },
        },
      },
      children: docChildren,
    },
  ],
});

Packer.toBuffer(doc).then((buffer) => {
  fs.writeFileSync(outputPath, buffer);
  console.log(`Wrote ${outputPath}`);
});
