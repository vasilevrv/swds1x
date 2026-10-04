import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source = readFileSync(new URL('../zigbee2mqtt/relay_2ch.mjs', import.meta.url), 'utf8').replace(/^import .*;\r?\n/gm, '');
const substitutes = `
const chain = () => new Proxy({}, {get: () => () => chain()});
const e = {binary: chain, action: chain};
const ea = {ALL: 7};
const m = {deviceAddCustomCluster: (name, definition) => ({name, definition}), deviceEndpoints: (args) => args, onOff: (args) => args};
`;
const {default: definition} = await import(`data:text/javascript;base64,${Buffer.from(substitutes + source).toString('base64')}`);
assert.deepEqual(definition.fingerprint, [{modelID: 'swrelay2x', manufacturerName: 'Salu Workshop'}]);
assert.equal(definition.ota, true);
assert.deepEqual(definition.extend[1].endpoints, {l1: 1, l2: 2});
assert.deepEqual(definition.extend[2].endpointNames, ['l1', 'l2']);
const custom = definition.extend[0].definition;
assert.equal(custom.ID, 0xfc00);
assert.equal(custom.attributes.buttonLinked.type, 0x10);
assert.equal(custom.attributes.doubleClickEnabled.type, 0x10);
assert.equal(custom.commandsResponse.buttonEvent.parameters[0].type, 0x30);
const [config, actions] = definition.fromZigbee;
assert.deepEqual(config.convert(null, {endpoint: {ID: 1}, data: {buttonLinked: 1, doubleClickEnabled: false}}), {button_1_linked: true, button_1_double_click: false});
assert.deepEqual(config.convert(null, {endpoint: {ID: 2}, data: {0: 0, 1: 1}}), {button_2_linked: false, button_2_double_click: true});
assert.deepEqual(config.convert(null, {endpoint: {ID: 2}, data: {buttonLinked: 2, doubleClickEnabled: null}}), {});
for (const endpoint of [1, 2]) for (const [event, name] of ['single', 'double', 'hold'].entries()) {
    const message = {endpoint: {ID: endpoint}, data: {event}};
    const expected = {action: `button_${endpoint}_${name}`};
    assert.deepEqual(actions.convert(null, message), expected);
    assert.deepEqual(actions.convert(null, message), expected); // Identical successive events remain events.
}
assert.equal(actions.convert(null, {endpoint: {ID: 3}, data: {event: 0}}), undefined);
for (const event of [-1, 3, null, '0', NaN]) assert.equal(actions.convert(null, {endpoint: {ID: 1}, data: {event}}), undefined);
const setter = definition.toZigbee[0];
const calls = [];
let readback = false;
let registered = false;
const device = {addCustomCluster: (name, definition) => {
    assert.equal(name, 'relay2Config'); assert.equal(definition.ID, 0xfc00); registered = true;
}, getEndpoint: (id) => ({
    write: async (...args) => {assert(registered); calls.push(['write', id, ...args]);},
    read: async (cluster, attributes) => {assert(registered); calls.push(['read', id, cluster, attributes]); return {[attributes[0]]: readback};},
})};
for (const key of setter.key) {
    calls.length = 0; readback = true; registered = false;
    assert.deepEqual(await setter.convertSet(null, key, true, {device}), {state: {[key]: true}});
    assert.equal(calls[0][1], key.includes('_1_') ? 1 : 2);
    assert.deepEqual(calls[0][3], {[key.endsWith('_linked') ? 'buttonLinked' : 'doubleClickEnabled']: true});
    registered = false; await setter.convertGet(null, key, {device});
    for (const invalid of ['true', 1, null, {}, NaN]) await assert.rejects(setter.convertSet(null, key, invalid, {device}));
}
readback = 2; await assert.rejects(setter.convertSet(null, 'button_1_linked', true, {device}));
calls.length = 0; await definition.configure({...device, addCustomCluster: () => {}});
assert.deepEqual(calls.map((call) => call[1]), [1, 2]);
console.log('PASS: converter identity, endpoints, Boolean settings/readback/routing, six button actions, repeated events, invalid values and initial configuration');
