(function () {
  "use strict";

  const schema = globalThis.StoryEditorSchema;
  const missionData = globalThis.StoryMissions;
  const paginate = globalThis.StoryPaginate;
  const validate = globalThis.StoryValidate;
  const $ = (id) => document.getElementById(id);
  const FONT_URL = "../fxdata/fonts/font_6x8.png";
  const ICON_URL = "../images/panels/berth_icons_16x8.png";
  const ROW_LABEL_URL = "../images/text/img_row_labels_128x8.png";

  const els = {
    open: $("open-folder"),
    save: $("save-all"),
    backup: $("backup"),
    reload: $("reload"),
    status: $("status"),
    warning: $("file-warning"),
    nav: $("scene-nav"),
    panel: $("scene-panel"),
    hints: $("hints-section"),
    table: $("hints-table"),
    station: $("station-preview"),
    stationCaption: $("station-caption"),
    errors: $("errors"),
    fallback: $("fallback-input"),
  };

  const state = {
    dir: null,
    storyDir: null,
    files: null,
    art: null,
    artUrl: null,
    dirty: { scenes: false, missions: false, hints: false },
    selectedScene: schema && schema.sceneOrder ? schema.sceneOrder[0] : "",
    selectedHint: schema && schema.berths && schema.points ? { berth: schema.berths[0].id, point: schema.points[0].id } : null,
    assets: { font: null, icons: null, rowLabels: null, previewArt: {} },
    saveMode: "directory",
  };

  let saveConfirmed = false;

  if (!schema || !missionData || !paginate || !validate) {
    if (els.warning) {
      els.warning.hidden = false;
      els.warning.textContent = "Generated editor data is missing. Run make gen.";
    }
    return;
  }

  function setStatus(text) {
    if (els.status) els.status.textContent = text;
  }

  function setWarning(text) {
    if (!els.warning) return;
    els.warning.hidden = !text;
    els.warning.textContent = text || "";
  }

  function anyDirty() {
    return state.dirty.scenes || state.dirty.missions || state.dirty.hints;
  }

  function updateSaveButton() {
    const downloadMode = state.saveMode === "download";
    els.save.disabled = !state.files || (!downloadMode && !anyDirty());
    els.backup.disabled = !state.files;
    els.reload.disabled = !state.files;
    els.save.textContent = downloadMode ? "Download JSON" : "Save all";
  }

  function markDirty(file) {
    state.dirty[file] = true;
    updateSaveButton();
  }

  function loadImage(url) {
    return new Promise((resolve, reject) => {
      const image = new Image();
      image.onload = () => resolve(image);
      image.onerror = () => reject(new Error("could not load " + url));
      image.src = url;
    });
  }

  async function loadImageFromFile(file) {
    const url = URL.createObjectURL(file);
    const image = await loadImage(url);
    return { image, url };
  }

  async function loadAssets() {
    const [font, icons, rowLabels] = await Promise.all([loadImage(FONT_URL), loadImage(ICON_URL), loadImage(ROW_LABEL_URL)]);
    state.assets = { font, icons, rowLabels, previewArt: {} };
    const animation = globalThis.StoryAnimation;
    if (animation && animation.scenes) {
      const entries = Object.entries(animation.scenes);
      const loaded = await Promise.all(entries.map(async ([name, frames]) => {
        try {
          return [name, await loadImage(frames[0])];
        } catch (error) {
          return null;
        }
      }));
      for (const entry of loaded) if (entry) state.assets.previewArt[entry[0]] = entry[1];
    }
  }

  function drawText(ctx, font, text, x, y) {
    [...String(text)].forEach((char, index) => {
      ctx.drawImage(font, (char.charCodeAt(0) & 255) * 6, 0, 6, 8, x + index * 6, y, 6, 8);
    });
  }

  function drawArt(canvas, artName) {
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    if (state.art) {
      const row = schema.art.rows.indexOf(artName);
      if (row >= 0) {
        ctx.drawImage(state.art, 0, row * schema.art.tileHeight, schema.art.tileWidth, schema.art.tileHeight, 0, 0, canvas.width, canvas.height);
        return;
      }
    }
    const preview = state.assets.previewArt && state.assets.previewArt[artName];
    if (preview) ctx.drawImage(preview, 0, 0, canvas.width, canvas.height);
  }

  function drawStoryPage(canvas, rows) {
    const width = canvas.width;
    const height = canvas.height;
    const ctx = canvas.getContext("2d");
    const image = ctx.createImageData(width, height);
    const bits = paginate.textFrame(rows, schema.glyphs, width, height);
    for (let index = 0; index < bits.length; ++index) {
      const value = bits[index] ? 255 : 0;
      const offset = index * 4;
      image.data[offset] = value;
      image.data[offset + 1] = value;
      image.data[offset + 2] = value;
      image.data[offset + 3] = 255;
    }
    ctx.putImageData(image, 0, 0);
  }

  function renderTextPages(container, paragraphs) {
    container.replaceChildren();
    let pages;
    try {
      pages = paginate.paginate(paragraphs, schema.wrapColumns, schema.rowsPerCard, schema.glyphs);
    } catch (error) {
      const message = document.createElement("p");
      message.className = "error";
      message.textContent = error.message;
      container.append(message);
      return;
    }
    pages.forEach((rows) => {
      const canvas = document.createElement("canvas");
      canvas.width = schema.art.tileWidth;
      canvas.height = schema.art.tileHeight;
      drawStoryPage(canvas, rows);
      container.append(canvas);
    });
  }

  function selectedScene(sourceScene) {
    return state.files.scenes.scenes.find((scene) => scene.name === sourceScene) || null;
  }

  function missionForScene(sceneId) {
    const fixed = schema.scenes.find((scene) => scene.id === sceneId);
    if (!fixed || !fixed.mission) return null;
    return state.files.missions.missions.find((mission) => mission.id === fixed.mission) || null;
  }

  function renderNav() {
    if (!state.files) return;
    els.nav.replaceChildren();
    const groups = state.files.missions.missions.map((mission) => ({ title: mission.title, scenes: mission.scenes }));
    groups.push({ title: "Interludes", scenes: state.files.missions.unassigned || [] });
    groups.forEach((group) => {
      if (!group.scenes.length) return;
      const section = document.createElement("div");
      section.className = "group";
      const title = document.createElement("div");
      title.className = "group-title";
      title.textContent = group.title;
      section.append(title);
      group.scenes.forEach((sceneId) => {
        const scene = selectedScene(sceneId);
        if (!scene) return;
        const button = document.createElement("button");
        button.type = "button";
        button.className = "scene";
        button.textContent = scene.title;
        button.setAttribute("aria-current", String(sceneId === state.selectedScene));
        button.addEventListener("click", () => {
          state.selectedScene = sceneId;
          renderNav();
          renderScene();
        });
        section.append(button);
      });
      els.nav.append(section);
    });
  }

  function fieldLabel(text) {
    const label = document.createElement("label");
    label.textContent = text;
    return label;
  }

  function renderMissionBox(fixedScene) {
    const box = document.createElement("div");
    box.className = "mission-box";
    const mission = missionForScene(fixedScene.id);
    if (!mission) {
      const note = document.createElement("p");
      note.className = "muted";
      note.textContent = "Interlude: no mission summary. This scene is grouped outside the eight missions.";
      box.append(note);
      return box;
    }
    const titleLabel = fieldLabel("Mission title");
    const title = document.createElement("input");
    title.type = "text";
    title.value = mission.title;
    title.addEventListener("input", () => {
      mission.title = title.value;
      markDirty("missions");
      renderNav();
    });
    box.append(titleLabel, title);
    const summaryLabel = fieldLabel("Mission summary");
    const summary = document.createElement("textarea");
    summary.value = mission.summary;
    summary.addEventListener("input", () => {
      mission.summary = summary.value;
      markDirty("missions");
    });
    const intentLabel = fieldLabel("Sequence intent");
    const intent = document.createElement("textarea");
    intent.value = mission.intent;
    intent.addEventListener("input", () => {
      mission.intent = intent.value;
      markDirty("missions");
    });
    box.append(summaryLabel, summary, intentLabel, intent);
    return box;
  }

  function imageCard(fixedCard) {
    const card = document.createElement("div");
    card.className = "card image";
    const figure = document.createElement("figure");
    figure.style.margin = "0";
    const canvas = document.createElement("canvas");
    canvas.className = "native";
    canvas.width = schema.art.tileWidth;
    canvas.height = schema.art.tileHeight;
    drawArt(canvas, fixedCard.art);
    const caption = document.createElement("figcaption");
    caption.className = "caption";
    caption.textContent = "IMAGE · " + fixedCard.art + " · pose 0";
    figure.append(canvas, caption);
    const note = document.createElement("p");
    note.className = "muted";
    note.textContent = "Art is read-only here. Replace the row in story/art.png for image edits.";
    card.append(figure, note);
    return card;
  }

  function textCard(sourceCard, fixedCard) {
    const card = document.createElement("div");
    card.className = "card";
    const preview = document.createElement("div");
    preview.className = "page-strip";
    const fields = document.createElement("div");
    sourceCard.paragraphs.forEach((paragraph, index) => {
      const wrapper = document.createElement("div");
      wrapper.className = "paragraph";
      const label = fieldLabel("Paragraph " + (index + 1));
      const input = document.createElement("textarea");
      input.value = paragraph;
      input.addEventListener("input", () => {
        sourceCard.paragraphs[index] = input.value;
        markDirty("scenes");
        renderTextPages(preview, sourceCard.paragraphs);
      });
      wrapper.append(label, input);
      fields.append(wrapper);
    });
    renderTextPages(preview, sourceCard.paragraphs);
    card.append(preview, fields);
    return card;
  }

  function renderScene() {
    if (!state.files) return;
    const source = selectedScene(state.selectedScene);
    const fixed = schema.scenes.find((scene) => scene.id === state.selectedScene);
    if (!source || !fixed) {
      els.panel.replaceChildren();
      return;
    }
    els.panel.replaceChildren();
    const heading = document.createElement("h2");
    heading.textContent = "Scene";
    const titleRow = document.createElement("div");
    titleRow.className = "title-row";
    const title = document.createElement("input");
    title.type = "text";
    title.value = source.title;
    title.addEventListener("input", () => {
      source.title = title.value;
      markDirty("scenes");
      renderNav();
    });
    titleRow.append(title);
    els.panel.append(heading, titleRow, renderMissionBox(fixed));
    const cards = document.createElement("div");
    cards.className = "cards";
    fixed.cards.forEach((fixedCard, index) => {
      const sourceCard = source.cards[index];
      if (!sourceCard || sourceCard.kind !== fixedCard.kind) return;
      cards.append(fixedCard.kind === "image" ? imageCard(fixedCard) : textCard(sourceCard, fixedCard));
    });
    els.panel.append(cards);
    els.hints.hidden = false;
  }

  function selectedHintText() {
    if (!state.selectedHint) return "";
    const rows = state.files.hints.berths || {};
    const row = rows[state.selectedHint.berth] || {};
    return row[state.selectedHint.point] || "";
  }

  function drawStation() {
    const ctx = els.station.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, 128, 64);
    const berthIndex = schema.berths.findIndex((berth) => berth.id === state.selectedHint.berth);
    const berth = schema.berths[berthIndex];
    const point = schema.points.find((item) => item.id === state.selectedHint.point);
    if (!berth || !point || !state.assets.font) return;
    drawText(ctx, state.assets.font, berth.label, 2, 2);
    if (state.assets.icons) ctx.drawImage(state.assets.icons, berthIndex * 16, 0, 16, 8, 110, 2, 16, 8);
    ctx.fillStyle = "#fff";
    ctx.fillRect(0, 11, 128, 1);
    ctx.fillRect(0, 9, 1, 3);
    ctx.fillRect(127, 9, 1, 3);
    if (state.assets.rowLabels) {
      ctx.fillStyle = "#fff";
      ctx.fillRect(0, 22, 128, 7);
      ctx.drawImage(state.assets.rowLabels, 2 * 128, 0, 128, 8, 4, 22, 128, 8);
      ctx.drawImage(state.assets.rowLabels, 128, 0, 128, 8, 4, 29, 128, 8);
    }
    drawText(ctx, state.assets.font, selectedHintText(), 3, 56);
    els.stationCaption.textContent = berth.label + " · " + point.label + " — " + point.facts;
  }

  function renderHints() {
    if (!state.files) return;
    const thead = els.table.querySelector("thead");
    const tbody = els.table.querySelector("tbody");
    thead.replaceChildren();
    tbody.replaceChildren();
    const headRow = document.createElement("tr");
    const corner = document.createElement("th");
    corner.className = "berth";
    corner.textContent = "Berth";
    headRow.append(corner);
    schema.points.forEach((point) => {
      const th = document.createElement("th");
      th.textContent = point.label;
      headRow.append(th);
    });
    thead.append(headRow);
    const hintRows = state.files.hints.berths || (state.files.hints.berths = {});
    schema.berths.forEach((berth) => {
      const row = document.createElement("tr");
      const th = document.createElement("th");
      th.className = "berth";
      th.textContent = berth.label;
      row.append(th);
      schema.points.forEach((point) => {
        const td = document.createElement("td");
        const input = document.createElement("input");
        input.type = "text";
        input.maxLength = schema.hintMaxChars;
        const rowData = hintRows[berth.id] || (hintRows[berth.id] = {});
        input.value = rowData[point.id] || "";
        input.dataset.berth = berth.id;
        input.dataset.point = point.id;
        input.setAttribute("aria-label", berth.label + " " + point.label + " hint");
        input.addEventListener("focus", () => {
          state.selectedHint = { berth: berth.id, point: point.id };
          drawStation();
        });
        input.addEventListener("input", () => {
          input.value = input.value.toUpperCase();
          state.files.hints.berths[berth.id][point.id] = input.value;
          markDirty("hints");
          drawStation();
        });
        td.append(input);
        row.append(td);
      });
      tbody.append(row);
    });
    drawStation();
  }

  function showErrors(errors) {
    if (!els.errors) return;
    els.errors.replaceChildren();
    els.errors.hidden = errors.length === 0;
    if (!errors.length) return;
    const heading = document.createElement("strong");
    heading.textContent = "Fix before saving:";
    const list = document.createElement("ul");
    errors.forEach((error) => {
      const item = document.createElement("li");
      item.textContent = error;
      list.append(item);
    });
    els.errors.append(heading, list);
  }

  function runValidation() {
    const errors = validate.validateAll(state.files, schema);
    showErrors(errors);
    return errors;
  }

  function applyLoadedFiles(files, art) {
    state.files = files;
    state.art = art ? art.image : null;
    if (state.artUrl) URL.revokeObjectURL(state.artUrl);
    state.artUrl = art ? art.url : null;
    state.selectedScene = schema.sceneOrder.find((sceneId) => files.scenes.scenes.some((scene) => scene.name === sceneId)) || schema.sceneOrder[0];
    state.selectedHint = { berth: schema.berths[0].id, point: schema.points[0].id };
    state.dirty = { scenes: false, missions: false, hints: false };
    saveConfirmed = false;
    renderNav();
    renderScene();
    renderHints();
    const errors = runValidation();
    updateSaveButton();
    setStatus(errors.length ? "Loaded with validation errors." : "Loaded. Edit text, then save and run make gen.");
  }

  function parseJson(text, name) {
    try {
      return JSON.parse(text);
    } catch (error) {
      throw new Error(name + ": " + error.message);
    }
  }

  async function readText(dir, name) {
    const handle = await dir.getFileHandle(name);
    return (await handle.getFile()).text();
  }

  async function readImage(dir, name) {
    const handle = await dir.getFileHandle(name);
    return loadImageFromFile(await handle.getFile());
  }

  async function hasFile(dir, name) {
    try {
      await dir.getFileHandle(name);
      return true;
    } catch (error) {
      return false;
    }
  }

  async function resolveStoryDir(root) {
    if (await hasFile(root, "scenes.json")) return root;
    const story = await root.getDirectoryHandle("story");
    if (await hasFile(story, "scenes.json")) return story;
    throw new Error("Select the repository root or the story folder.");
  }

  async function loadFromDirectory(root) {
    const storyDir = await resolveStoryDir(root);
    const [scenesText, missionsText, hintsText] = await Promise.all([
      readText(storyDir, "scenes.json"),
      readText(storyDir, "missions.json"),
      readText(storyDir, "dock_hints.json"),
    ]);
    const files = {
      scenes: parseJson(scenesText, "scenes.json"),
      missions: parseJson(missionsText, "missions.json"),
      hints: parseJson(hintsText, "dock_hints.json"),
    };
    let art = null;
    try {
      art = await readImage(storyDir, files.scenes.art.file);
    } catch (error) {
      setWarning("art.png could not be loaded; image previews use the generated story art.");
    }
    state.dir = root;
    state.storyDir = storyDir;
    state.saveMode = "directory";
    applyLoadedFiles(files, art);
  }

  async function loadFromFileList(fileList) {
    const byName = new Map([...fileList].map((file) => [file.name, file]));
    const need = ["scenes.json", "missions.json", "dock_hints.json"];
    const missing = need.filter((name) => !byName.has(name));
    if (missing.length) throw new Error("Missing " + missing.join(", "));
    const files = {
      scenes: parseJson(await byName.get("scenes.json").text(), "scenes.json"),
      missions: parseJson(await byName.get("missions.json").text(), "missions.json"),
      hints: parseJson(await byName.get("dock_hints.json").text(), "dock_hints.json"),
    };
    const artFile = byName.get("art.png");
    let art = null;
    if (artFile) {
      try {
        art = await loadImageFromFile(artFile);
      } catch (error) {
        setWarning("art.png could not be loaded; image previews use the generated story art.");
      }
    }
    state.dir = null;
    state.storyDir = null;
    state.saveMode = "download";
    applyLoadedFiles(files, art);
    const artNote = art ? "" : (artFile
      ? " art.png could not be read; image previews use the generated story art."
      : " art.png was not selected; image previews use the generated story art.");
    setWarning("File System Access API unavailable. Use Chrome/Edge (or run make editor) for save-in-place, or load the four files here and download canonical JSON to replace story/*.json." + artNote);
  }

  async function writeFile(dir, name, text) {
    const handle = await dir.getFileHandle(name, { create: true });
    const writable = await handle.createWritable();
    await writable.write(text);
    await writable.close();
  }

  function jsonText(value) {
    return JSON.stringify(value, null, 2) + "\n";
  }

  function download(name, text) {
    const blob = new Blob([text], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = name;
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }

  function backupAll() {
    if (!state.files) return;
    download("scenes.backup.json", jsonText(state.files.scenes));
    download("missions.backup.json", jsonText(state.files.missions));
    download("dock_hints.backup.json", jsonText(state.files.hints));
    setStatus("Backup JSON downloaded.");
  }

  async function saveAll() {
    if (!state.files) return;
    const errors = runValidation();
    if (errors.length) {
      setStatus("Not saved: fix the validation errors.");
      return;
    }
    const downloadMode = state.saveMode === "download";
    const jobs = [];
    if (downloadMode || state.dirty.scenes) jobs.push(["scenes.json", jsonText(state.files.scenes), "scenes"]);
    if (downloadMode || state.dirty.missions) jobs.push(["missions.json", jsonText(state.files.missions), "missions"]);
    if (downloadMode || state.dirty.hints) jobs.push(["dock_hints.json", jsonText(state.files.hints), "hints"]);
    if (!jobs.length) {
      setStatus("Nothing to save.");
      return;
    }
    if (state.storyDir && !saveConfirmed) {
      if (!confirm("Overwrite the canonical story JSON files?")) return;
      saveConfirmed = true;
    }
    try {
      for (const [name, text, key] of jobs) {
        if (state.storyDir) await writeFile(state.storyDir, name, text);
        else download(name, text);
        state.dirty[key] = false;
      }
      updateSaveButton();
      setStatus(downloadMode
        ? "Downloaded scenes.json, missions.json and dock_hints.json. Replace story/*.json, then run make gen."
        : "Saved. Run make gen to refresh the preview and packed story.");
    } catch (error) {
      setStatus("Save failed: " + error.message);
    }
  }

  async function openDirectory() {
    if (!window.showDirectoryPicker) {
      els.fallback.click();
      return;
    }
    try {
      const root = await window.showDirectoryPicker({ mode: "readwrite" });
      await loadFromDirectory(root);
      rememberDirectory(root);
    } catch (error) {
      if (error && error.name !== "AbortError") setStatus("Open failed: " + error.message);
    }
  }

  function initFallback() {
    if (window.showDirectoryPicker) return;
    state.saveMode = "download";
    els.open.textContent = "Open story files";
    setWarning("This browser has no directory write API. Use Chrome/Edge (or run make editor) for save-in-place, or load the four files here and download the edited JSON.");
  }

  const DB_NAME = "sub-hauler-story-editor";
  const STORE = "handles";
  function openDb() {
    return new Promise((resolve, reject) => {
      const request = indexedDB.open(DB_NAME, 1);
      request.onupgradeneeded = () => request.result.createObjectStore(STORE);
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
    });
  }
  async function rememberDirectory(handle) {
    try {
      const db = await openDb();
      await new Promise((resolve, reject) => {
        const tx = db.transaction(STORE, "readwrite");
        tx.objectStore(STORE).put(handle, "story");
        tx.oncomplete = resolve;
        tx.onerror = () => reject(tx.error);
      });
      db.close();
    } catch (error) {
      /* optional convenience only */
    }
  }
  async function recallDirectory() {
    try {
      const db = await openDb();
      const handle = await new Promise((resolve, reject) => {
        const tx = db.transaction(STORE, "readonly");
        const request = tx.objectStore(STORE).get("story");
        request.onsuccess = () => resolve(request.result || null);
        request.onerror = () => reject(request.error);
      });
      db.close();
      return handle;
    } catch (error) {
      return null;
    }
  }

  async function init() {
    initFallback();
    updateSaveButton();
    try {
      await loadAssets();
    } catch (error) {
      setWarning("Preview assets failed: " + error.message);
    }
    els.open.addEventListener("click", openDirectory);
    els.save.addEventListener("click", saveAll);
    els.backup.addEventListener("click", backupAll);
    els.reload.addEventListener("click", async () => {
      if (!state.storyDir) {
        if (anyDirty() && !confirm("Discard unsaved changes?")) return;
        els.open.click();
        return;
      }
      if (anyDirty() && !confirm("Discard unsaved changes?")) return;
      try {
        await loadFromDirectory(state.dir);
      } catch (error) {
        setStatus("Reload failed: " + error.message);
      }
    });
    els.fallback.addEventListener("change", async () => {
      try {
        if (els.fallback.files.length) await loadFromFileList(els.fallback.files);
      } catch (error) {
        setStatus("Load failed: " + error.message);
      }
    });
    window.addEventListener("beforeunload", (event) => {
      if (!anyDirty()) return;
      event.preventDefault();
      event.returnValue = "";
    });
    const remembered = await recallDirectory();
    if (remembered && remembered.queryPermission && (await remembered.queryPermission({ mode: "readwrite" })) === "granted") {
      try {
        await loadFromDirectory(remembered);
        return;
      } catch (error) {
        /* fall through to the empty state */
      }
    }
    setStatus("Open the repository root or the story folder.");
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
})();
