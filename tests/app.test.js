const assert = require('node:assert/strict');
const { DEFAULT_API_BASE, getApiBase, isDeltaValid } = require('../app.js');

function memoryStorage(initial = {}) {
  const data = { ...initial };
  return {
    getItem(key) {
      return Object.prototype.hasOwnProperty.call(data, key) ? data[key] : null;
    },
    setItem(key, value) {
      data[key] = value;
    }
  };
}

assert.equal(
  getApiBase({ origin: 'http://192.168.1.50', hostname: '192.168.1.50', search: '' }),
  'http://192.168.1.50'
);

const storage = memoryStorage();
assert.equal(
  getApiBase(
    { origin: 'https://kbecmt.github.io', hostname: 'kbecmt.github.io', search: '?api=http://192.168.1.77/' },
    storage
  ),
  'http://192.168.1.77'
);
assert.equal(storage.getItem('solarApiBase'), 'http://192.168.1.77/');

assert.equal(
  getApiBase(
    { origin: 'https://kbecmt.github.io', hostname: 'kbecmt.github.io', search: '' },
    memoryStorage({ solarApiBase: 'http://192.168.1.88/' })
  ),
  'http://192.168.1.88'
);

assert.equal(getApiBase(null, null), DEFAULT_API_BASE);

assert.equal(isDeltaValid(8, 4), true);
assert.equal(isDeltaValid(4, 4), false);
assert.equal(isDeltaValid(3, 4), false);
assert.equal(isDeltaValid(Number.NaN, 1), false);

console.log('app tests ok');
