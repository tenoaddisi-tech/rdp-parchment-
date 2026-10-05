const api = 'http://127.0.0.1:8787/api';
const status = document.querySelector('#status');
const hardware = document.querySelector('#hardware');
const generation = document.querySelector('#generation');
const total = document.querySelector('#total');
const message = document.querySelector('#message');
const generateButton = document.querySelector('#generate');
const reseedButton = document.querySelector('#reseed');
const copyButton = document.querySelector('#copy');
const valuesList = document.querySelector('#values');
const form = document.querySelector('#generate-form');
let latestValues = [];

function showError(error) {
  message.textContent = error instanceof Error ? error.message : String(error);
  message.hidden = false;
}

function clearError() {
  message.textContent = '';
  message.hidden = true;
}

async function requestJson(path, options) {
  const response = await fetch(`${api}${path}`, options);
  const data = await response.json();
  if (!response.ok || data.error) {
    throw new Error(data.error || `Request failed: ${response.status}`);
  }
  return data;
}

function applyStatus(data) {
  status.dataset.state = data.ready ? 'ready' : 'error';
  status.textContent = data.ready ? 'Ready' : 'Unavailable';
  hardware.textContent = data.rdrandSupported ? 'RDRAND available' : 'Unavailable';
  generation.textContent = String(data.generation);
  total.textContent = Number(data.valuesGenerated).toLocaleString();
  generateButton.disabled = !data.ready;
  reseedButton.disabled = !data.rdrandSupported;
  if (data.error) showError(data.error);
}

async function refreshStatus() {
  try {
    applyStatus(await requestJson('/status'));
  } catch (error) {
    status.dataset.state = 'error';
    status.textContent = 'App not running';
    generateButton.disabled = true;
    reseedButton.disabled = true;
    showError('Start rdrand_dashboard.exe, then reopen this extension.');
  }
}

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  clearError();
  generateButton.disabled = true;
  try {
    const count = document.querySelector('#count').value;
    const bound = document.querySelector('#bound').value;
    const data = await requestJson(`/random?count=${encodeURIComponent(count)}&bound=${encodeURIComponent(bound)}`);
    latestValues = data.values;
    valuesList.replaceChildren(...latestValues.map((value) => {
      const item = document.createElement('li');
      item.textContent = value;
      return item;
    }));
    generation.textContent = String(data.generation);
    total.textContent = Number(data.valuesGenerated).toLocaleString();
    copyButton.disabled = false;
  } catch (error) {
    showError(error);
  } finally {
    generateButton.disabled = false;
  }
});

reseedButton.addEventListener('click', async () => {
  clearError();
  reseedButton.disabled = true;
  try {
    applyStatus(await requestJson('/reseed', { method: 'POST' }));
  } catch (error) {
    showError(error);
  } finally {
    if (status.dataset.state === 'ready') reseedButton.disabled = false;
  }
});

copyButton.addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText(latestValues.join('\n'));
  } catch {
    showError('Clipboard access was denied by the browser.');
  }
});

refreshStatus();
