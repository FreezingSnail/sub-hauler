'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const paginate = require('../../docs/assets/story-editor/paginate.js');

const repo = path.resolve(__dirname, '../..');
const source = JSON.parse(fs.readFileSync(path.join(repo, 'story/scenes.json'), 'utf8'));
const context = {};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(repo, 'docs/assets/first-beat-animation/frames.js'), 'utf8'), context);
const glyphs = context.StoryData.glyphs;
const columns = context.StoryData.columns;
const rowsPerCard = context.StoryData.rows;

test('JS pagination matches every packed story text card', () => {
  for (const scene of source.scenes) {
    const generated = context.StoryData.scenes.find((item) => item.id === scene.name);
    assert.ok(generated, scene.name);
    const expected = [];
    for (const card of scene.cards) {
      if (card.kind !== 'text') continue;
      expected.push(...paginate.paginate(card.paragraphs, columns, rowsPerCard, glyphs));
    }
    const actual = generated.cards.filter((card) => card.kind === 'text').map((card) => Array.from(card.rows));
    assert.deepEqual(JSON.parse(JSON.stringify(actual)), JSON.parse(JSON.stringify(expected)), scene.name);
  }
});

test('pagination inserts a blank row between paragraphs and splits full pages', () => {
  assert.deepEqual(paginate.paginate(['A', 'B'], columns, rowsPerCard, glyphs), [['A', '', 'B']]);
  const pages = paginate.paginate(Array.from({ length: 7 }, (_, index) => String.fromCharCode(65 + index)), columns, 6, glyphs);
  assert.deepEqual(pages, [['A', '', 'B', '', 'C', ''], ['D', '', 'E', '', 'F', ''], ['G']]);
});

test('validation rejects overlong words and unsupported characters', () => {
  assert.throws(() => paginate.wrapParagraph('A'.repeat(columns + 1), columns, glyphs), /exceeds/);
  assert.throws(() => paginate.wrapParagraph('HELLO!', columns, glyphs), /unsupported/);
});

test('textFrame renders only inside the native 128x64 page', () => {
  const width = 128;
  const height = 64;
  const pixels = paginate.textFrame(['THE SUB ARRIVES AT', 'BENTHOS WITH FILTER'], glyphs, width, height);
  assert.equal(pixels.length, width * height);
  assert.ok(pixels.some((value) => value === 1));
  assert.ok(pixels.every((value) => value === 0 || value === 1));
});

const validate = require('../../docs/assets/story-editor/validate.js');
const editorContext = {};
vm.createContext(editorContext);
vm.runInContext(fs.readFileSync(path.join(repo, 'docs/assets/story-editor/schema.js'), 'utf8'), editorContext);
const editorSchema = editorContext.StoryEditorSchema;
const currentFiles = () => ({
  scenes: JSON.parse(fs.readFileSync(path.join(repo, 'story/scenes.json'), 'utf8')),
  missions: JSON.parse(fs.readFileSync(path.join(repo, 'story/missions.json'), 'utf8')),
  hints: JSON.parse(fs.readFileSync(path.join(repo, 'story/dock_hints.json'), 'utf8')),
});

test('current sources pass editor validation', () => {
  assert.deepEqual(validate.validateAll(currentFiles(), editorSchema), []);
});

test('editor validation rejects structural and text changes', () => {
  const files = currentFiles();
  files.scenes.scenes[0].title = '';
  files.missions.missions[0].summary = '';
  files.scenes.scenes[0].cards.pop();
  const errors = validate.validateAll(files, editorSchema);
  assert.ok(errors.some((error) => error.includes('empty title')));
  assert.ok(errors.some((error) => error.includes('empty summary')));
  assert.ok(errors.some((error) => error.includes('card count changed')));
});

test('editor validation allows duplicate hints and rejects bad text', () => {
  const duplicate = currentFiles();
  duplicate.hints.berths.tidewall.fresh = duplicate.hints.berths.benthos.fresh;
  assert.deepEqual(validate.validateHints(duplicate.hints, editorSchema), []);

  const files = currentFiles();
  files.hints.berths.benthos.pump = 'lowercase';
  files.hints.berths.cormorant.pump = 'X'.repeat(editorSchema.hintMaxChars + 1);
  const errors = validate.validateAll(files, editorSchema);
  assert.ok(errors.some((error) => error.includes('uppercase')));
  assert.ok(errors.some((error) => error.includes('exceeds')));
});

test('editor page loads the generated schema and fixed text controls', () => {
  const html = fs.readFileSync(path.join(repo, 'docs/story-editor.html'), 'utf8');
  for (const asset of ['assets/story-editor/schema.js', 'assets/story-editor/missions.js', 'assets/first-beat-animation/frames.js', 'assets/story-editor/paginate.js', 'assets/story-editor/validate.js', 'assets/story-editor/editor.js'])
    assert.ok(html.includes(asset), asset);
  for (const id of ['open-folder', 'save-all', 'reload', 'scene-nav', 'scene-panel', 'station-preview', 'hints-table'])
    assert.ok(html.includes('id="' + id + '"'), id);
  assert.equal(html.includes('<textarea id="json"'), false);
});

test('generated editor schema matches the fixed source shape', () => {
  assert.equal(editorSchema.scenes.length, source.scenes.length);
  assert.equal(editorSchema.missions.length, 8);
  assert.equal(editorSchema.berths.length, 8);
  assert.equal(editorSchema.points.length, 9);
  assert.match('FILTERS: TAKE WORK', new RegExp(editorSchema.hintPattern));
});
