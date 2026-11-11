(function (root, factory) {
  if (typeof module === "object" && module.exports) module.exports = factory();
  else root.StoryPaginate = factory();
})(globalThis, function () {
  "use strict";

  function normalizeProse(paragraph) {
    const text = String(paragraph).split(/\s+/).filter(Boolean).join(" ").toUpperCase();
    if (!text) throw new Error("paragraph is empty");
    return text;
  }

  function unsupportedCharacters(text, glyphs) {
    const unsupported = [];
    for (const char of text) {
      if (!Object.prototype.hasOwnProperty.call(glyphs, char) && !unsupported.includes(char))
        unsupported.push(char);
    }
    return unsupported;
  }

  function wrapParagraph(paragraph, columns, glyphs) {
    const text = normalizeProse(paragraph);
    const unsupported = unsupportedCharacters(text, glyphs);
    if (unsupported.length)
      throw new Error("unsupported character(s): " + unsupported.map((char) => JSON.stringify(char)).join(", "));
    const rows = [];
    let row = "";
    for (const word of text.split(" ")) {
      if (word.length > columns) throw new Error("word " + JSON.stringify(word) + " exceeds " + columns + " columns");
      if (!row) row = word;
      else if (row.length + 1 + word.length <= columns) row += " " + word;
      else {
        rows.push(row);
        row = word;
      }
    }
    if (row) rows.push(row);
    return rows;
  }

  function paginate(paragraphs, columns, rowsPerCard, glyphs) {
    const pages = [];
    let page = [];
    paragraphs.forEach((paragraph, index) => {
      const rows = wrapParagraph(paragraph, columns, glyphs);
      if (index !== 0) {
        if (page.length < rowsPerCard) page.push("");
        else if (page.length) {
          pages.push(page);
          page = [];
        }
      }
      for (const row of rows) {
        if (page.length >= rowsPerCard) {
          pages.push(page);
          page = [];
        }
        page.push(row);
      }
    });
    if (page.length) pages.push(page);
    return pages;
  }

  // Same pixel placement as tools/gen_story_beats.py:text_frame.
  function textFrame(rows, glyphs, width, height) {
    if (!rows.length || rows.length > 6) throw new Error("text card must have 1..6 rows");
    const pixels = new Uint8Array(width * height);
    const top = Math.floor((height - ((rows.length - 1) * 10 + 7)) / 2);
    rows.forEach((row, line) => {
      const left = Math.floor((width - (row.length * 6 - 1)) / 2);
      [...row].forEach((char, index) => {
        const glyph = glyphs[char];
        if (!glyph) throw new Error("missing glyph " + JSON.stringify(char));
        glyph.forEach((bits, column) => {
          for (let y = 0; y < 7; ++y) {
            if (!(bits & (1 << y))) continue;
            const pixelY = top + line * 10 + y;
            const pixelX = left + index * 6 + column;
            if (pixelY >= 0 && pixelY < height && pixelX >= 0 && pixelX < width)
              pixels[pixelY * width + pixelX] = 1;
          }
        });
      });
    });
    return pixels;
  }

  return { normalizeProse, unsupportedCharacters, wrapParagraph, paginate, textFrame };
});
