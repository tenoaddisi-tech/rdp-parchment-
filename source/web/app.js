const statusBox = document.querySelector('#status');
const statusText = document.querySelector('#status-text');
const hardwareValue = document.querySelector('#hardware-value');
const generationValue = document.querySelector('#generation-value');
const totalValue = document.querySelector('#total-value');
const form = document.querySelector('#generate-form');
const generateButton = document.querySelector('#generate-button');
const reseedButton = document.querySelector('#reseed-button');
const copyButton = document.querySelector('#copy-button');
const valuesList = document.querySelector('#values');
const message = document.querySelector('#message');
const format = document.querySelector('#format');

let latestValues = [];

function showError(text) {
  message.textContent = text;
  message.hidden = false;
}

function clearError() {
  message.hidden = true;
  message.textContent = '';
}

function applyStatus(data) {
  hardwareValue.textContent = data.rdrandSupported ? 'RDRAND available' : 'Unavailable';
  generationValue.textContent = String(data.generation ?? 0);
  totalValue.textContent = Number(data.valuesGenerated ?? 0).toLocaleString();
  statusBox.dataset.state = data.ready ? 'ready' : 'error';
  statusText.textContent = data.ready ? 'Generator ready' : 'Hardware unavailable';
  generateButton.disabled = !data.ready;
  reseedButton.disabled = !data.rdrandSupported;
  if (data.error) showError(data.error);
}

async function requestJson(url, options) {
  const response = await fetch(url, options);
  const data = await response.json();
  if (!response.ok || data.error) throw new Error(data.error || `Request failed: ${response.status}`);
  return data;
}

async function refreshStatus() {
  try {
    applyStatus(await requestJson('/api/status'));
  } catch (error) {
    statusBox.dataset.state = 'error';
    statusText.textContent = 'Service disconnected';
    generateButton.disabled = true;
    reseedButton.disabled = true;
    showError(error.message);
  }
}

function displayValue(value) {
  if (format.value === 'hex') {
    return `0x${BigInt(value).toString(16).padStart(16, '0')}`;
  }
  return value;
}

function renderValues() {
  valuesList.replaceChildren();
  for (const value of latestValues) {
    const item = document.createElement('li');
    const text = document.createElement('span');
    text.textContent = displayValue(value);
    item.append(text);
    valuesList.append(item);
  }
}

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  clearError();
  generateButton.disabled = true;
  generateButton.textContent = 'Generating…';
  const count = document.querySelector('#count').value;
  const bound = document.querySelector('#bound').value;
  try {
    const data = await requestJson(`/api/random?count=${encodeURIComponent(count)}&bound=${encodeURIComponent(bound)}`);
    latestValues = data.values;
    renderValues();
    generationValue.textContent = String(data.generation);
    totalValue.textContent = Number(data.valuesGenerated).toLocaleString();
    copyButton.disabled = false;
  } catch (error) {
    showError(error.message);
  } finally {
    generateButton.disabled = false;
    generateButton.textContent = 'Generate random values';
  }
});

reseedButton.addEventListener('click', async () => {
  clearError();
  reseedButton.disabled = true;
  reseedButton.textContent = 'Reseeding…';
  try {
    applyStatus(await requestJson('/api/reseed', { method: 'POST' }));
  } catch (error) {
    showError(error.message);
  } finally {
    reseedButton.textContent = 'Reseed from RDRAND';
    if (statusBox.dataset.state === 'ready') reseedButton.disabled = false;
  }
});

format.addEventListener('change', renderValues);

copyButton.addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText(latestValues.map(displayValue).join('\n'));
    copyButton.textContent = 'Copied';
    setTimeout(() => { copyButton.textContent = 'Copy values'; }, 1200);
  } catch {
    showError('Clipboard access was denied by the browser.');
  }
});

refreshStatus();
