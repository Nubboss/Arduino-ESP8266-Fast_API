const ENDPOINT = '/data';
const INTERVAL = 1000;

const elTemp = document.getElementById('temp');
const elHum = document.getElementById('hum');
const elTime = document.getElementById('time');

async function update() {
  try {
    const res = await fetch(ENDPOINT);
    if (!res.ok) throw 'network';
    const arr = await res.json();
    if (!Array.isArray(arr) || arr.length === 0) {
      elTemp.textContent = '— °C';
      elHum.textContent = '— %';
      elTime.textContent = 'Обновление: нет данных';
      return;
    }
    const last = arr[arr.length - 1];
    const t = last.temperature;
    const h = last.humidity;
    elTemp.textContent = (t == null) ? '— °C' : (t) + ' °C';
    elHum.textContent = (h == null) ? '— %' : (h) + ' %';
    elTime.textContent = 'Обновление: ' + (last.received_at || 'сейчас');
  } catch (e) {
    elTime.textContent = 'Ошибка загрузки';
  }
}

update();
setInterval(update, INTERVAL);