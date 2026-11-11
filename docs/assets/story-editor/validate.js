(function (root, factory) {
  const paginate = typeof module === "object" && module.exports ? require("./paginate.js") : root.StoryPaginate;
  if (typeof module === "object" && module.exports) module.exports = factory(paginate);
  else root.StoryValidate = factory(paginate);
})(globalThis, function (paginate) {
  "use strict";

  function validateScenes(document, schema) {
    const errors = [];
    if (!document || document.version !== 1) errors.push("scenes.json: expected version 1");
    if (!document || !document.art) {
      errors.push("scenes.json: missing art metadata");
      return errors;
    }
    if (document.art.file !== schema.art.file) errors.push("scenes.json: art file changed");
    if (document.art.tile_width !== schema.art.tileWidth || document.art.tile_height !== schema.art.tileHeight ||
        document.art.poses !== schema.art.poses) errors.push("scenes.json: art dimensions changed");
    if (!Array.isArray(document.art.rows) || document.art.rows.join("|") !== schema.art.rows.join("|"))
      errors.push("scenes.json: art rows changed");
    if (!Array.isArray(document.scenes)) {
      errors.push("scenes.json: scenes must be an array");
      return errors;
    }
    if (document.scenes.length !== schema.scenes.length)
      errors.push("scenes.json: expected " + schema.scenes.length + " fixed scenes");
    schema.scenes.forEach((fixed, index) => {
      const scene = document.scenes[index];
      if (!scene) {
        errors.push("scenes.json: missing scene " + fixed.id);
        return;
      }
      if (scene.name !== fixed.id) {
        errors.push("scenes.json: scene " + index + " is " + JSON.stringify(scene.name) + ", expected " + fixed.id);
        return;
      }
      if (typeof scene.title !== "string" || !scene.title.trim()) errors.push("scenes.json: " + fixed.id + " has an empty title");
      if (!Array.isArray(scene.cards) || scene.cards.length !== fixed.cards.length) {
        errors.push("scenes.json: " + fixed.id + " card count changed");
        return;
      }
      fixed.cards.forEach((fixedCard, cardIndex) => {
        const card = scene.cards[cardIndex];
        const where = "scenes.json: " + fixed.id + " card " + (cardIndex + 1);
        if (!card || card.kind !== fixedCard.kind) {
          errors.push(where + " kind changed");
          return;
        }
        if (fixedCard.kind === "image") {
          if (card.art !== fixedCard.art) errors.push(where + " art reference changed");
          return;
        }
        if (!Array.isArray(card.paragraphs) || card.paragraphs.length !== fixedCard.paragraphs) {
          errors.push(where + " paragraph count changed");
          return;
        }
        card.paragraphs.forEach((paragraph, paragraphIndex) => {
          if (typeof paragraph !== "string" || !paragraph.trim()) {
            errors.push(where + " paragraph " + (paragraphIndex + 1) + " is empty");
            return;
          }
          try {
            paginate.wrapParagraph(paragraph, schema.wrapColumns, schema.glyphs);
          } catch (error) {
            errors.push(where + " paragraph " + (paragraphIndex + 1) + ": " + error.message);
          }
        });
      });
    });
    return errors;
  }

  function validateMissions(document, schema) {
    const errors = [];
    if (!document || document.schema !== 1) errors.push("missions.json: expected schema 1");
    if (!document || !Array.isArray(document.missions)) {
      errors.push("missions.json: missions must be an array");
      return errors;
    }
    if (!Array.isArray(document.unassigned)) errors.push("missions.json: unassigned must be an array");
    const expected = new Map(schema.missions.map((mission) => [mission.id, mission]));
    if (document.missions.length !== expected.size) errors.push("missions.json: mission count changed");
    const seen = new Set();
    for (const mission of document.missions) {
      if (seen.has(mission.id)) errors.push("missions.json: duplicate mission " + mission.id);
      seen.add(mission.id);
      const fixed = expected.get(mission.id);
      if (!fixed) {
        errors.push("missions.json: unknown mission " + JSON.stringify(mission.id));
        continue;
      }
      for (const key of ["title", "summary", "intent"]) {
        if (typeof mission[key] !== "string" || !mission[key].trim()) errors.push("missions.json: " + mission.id + " has an empty " + key);
      }
      if (!Array.isArray(mission.scenes) || mission.scenes.length !== fixed.scenes.length ||
          mission.scenes.some((name, index) => name !== fixed.scenes[index])) {
        errors.push("missions.json: " + mission.id + " scene links changed");
      }
    }
    if ([...expected.keys()].some((id) => !seen.has(id))) errors.push("missions.json: a mission is missing");
    const unassigned = document.unassigned || [];
    if (unassigned.length !== schema.unassignedScenes.length ||
        unassigned.some((name, index) => name !== schema.unassignedScenes[index])) {
      errors.push("missions.json: unassigned scene list changed");
    }
    return errors;
  }

  function validateHints(document, schema) {
    const errors = [];
    if (!document || document.schema !== 1) errors.push("dock_hints.json: expected schema 1");
    if (!document || !Array.isArray(document.points) || !document.berths) {
      errors.push("dock_hints.json: missing points or berths");
      return errors;
    }
    const expectedPoints = schema.points.map((point) => point.id);
    if (document.points.length !== expectedPoints.length ||
        document.points.some((point, index) => point.id !== expectedPoints[index])) {
      errors.push("dock_hints.json: point order changed");
    }
    const pattern = new RegExp(schema.hintPattern);
    const extraBerths = Object.keys(document.berths).filter((id) => !schema.berths.some((berth) => berth.id === id));
    if (extraBerths.length) errors.push("dock_hints.json: unknown berths " + extraBerths.join(", "));
    for (const berth of schema.berths) {
      const row = document.berths[berth.id];
      if (!row) {
        errors.push("dock_hints.json: missing berth " + berth.id);
        continue;
      }
      for (const point of expectedPoints) {
        const value = row[point];
        const where = "dock_hints.json: " + berth.id + "/" + point;
        if (typeof value !== "string" || !value) {
          errors.push(where + " is empty");
          continue;
        }
        if (value.length > schema.hintMaxChars) errors.push(where + " exceeds " + schema.hintMaxChars + " characters");
        if (!pattern.test(value)) errors.push(where + " uses unsupported characters");
        if (value !== value.toUpperCase()) errors.push(where + " must be uppercase");
      }
      const extra = Object.keys(row).filter((point) => !expectedPoints.includes(point));
      if (extra.length) errors.push("dock_hints.json: " + berth.id + " has unknown points " + extra.join(", "));
    }
    return errors;
  }

  function validateAll(files, schema) {
    return [
      ...validateScenes(files.scenes, schema),
      ...validateMissions(files.missions, schema),
      ...validateHints(files.hints, schema),
    ];
  }

  return { validateScenes, validateMissions, validateHints, validateAll };
});
