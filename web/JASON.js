const API_URL = 'http://192.168.1.12/Api/get.php';
const REFRESH_INTERVAL = 60000;

const RANGES = [
	{ key: '1h', label: '1 giờ', duration: 60 * 60 * 1000 },
	{ key: '24h', label: '24 giờ', duration: 24 * 60 * 60 * 1000 },
	{ key: '7d', label: '7 ngày', duration: 7 * 24 * 60 * 60 * 1000 },
	{ key: '30d', label: '30 ngày', duration: 30 * 24 * 60 * 60 * 1000 },
];

const METRICS = [
	{ key: 'temp', label: 'Nhiệt độ', unit: '°C', color: '#C1502E' },
	{ key: 'humidity', label: 'Độ ẩm không khí', unit: '%', color: '#3E7CB4' },
	{ key: 'light', label: 'Ánh sáng', unit: 'klux', color: '#E8A33D' },
	{ key: 'soil', label: 'Độ ẩm đất', unit: '%', color: '#5C8A54' },
];

const FORECAST_CACHE_KEY = 'danang-weather-forecast';
const WEATHER_ICONS = {
	0: 'sun',
	1: 'cloud-sun', 2: 'cloud-sun', 3: 'cloud-sun',
	45: 'cloud-drizzle', 48: 'cloud-drizzle',
	51: 'cloud-drizzle', 53: 'cloud-drizzle', 55: 'cloud-drizzle',
	56: 'cloud-drizzle', 57: 'cloud-drizzle',
	61: 'cloud-rain', 63: 'cloud-rain', 65: 'cloud-rain',
	66: 'cloud-rain', 67: 'cloud-rain',
	71: 'cloud-drizzle', 73: 'cloud-drizzle', 75: 'cloud-rain', 77: 'cloud-drizzle',
	80: 'cloud-rain', 81: 'cloud-rain', 82: 'cloud-rain',
	85: 'cloud-rain', 86: 'cloud-rain',
	95: 'cloud-rain', 96: 'cloud-rain', 99: 'cloud-rain',
};

const ICONS = {
	sun: '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',
	'cloud-sun': '<path d="M12 2v1M4.2 4.2l.7.7M2 11h1M18.8 4.9l-.7.7"/><path d="M9 16a4 4 0 1 1 6.9-3.9A4.5 4.5 0 0 1 15.5 21H8a3 3 0 0 1-1-5.8"/>',
	'cloud-rain': '<path d="M16 13v8M8 13v8M12 15v8"/><path d="M20 16.6A5 5 0 0 0 18 7h-1.3A7 7 0 1 0 4 14.5"/>',
	'cloud-drizzle': '<path d="M8 19v1M8 14v1M16 19v1M16 14v1M12 21v1M12 16v1"/><path d="M20 12.6A5 5 0 0 0 18 3h-1.3A7 7 0 1 0 4 10.5"/>',
};

let activeMetrics = ['temp', 'humidity'];
let currentRange = '24h';
let chart;
let requestSequence = 0;
let currentRecords = [];

function formatDate(date) {
	const year = date.getFullYear();
	const month = String(date.getMonth() + 1).padStart(2, '0');
	const day = String(date.getDate()).padStart(2, '0');
	return `${year}-${month}-${day}`;
}

function parseSensorRecord(record) {
	const timestamp = new Date(String(record.dateupdate || '').replace(' ', 'T'));
	const temp = Number(record.tempt);
	const humidity = Number(record.humidity);
	const moisture = Number(record.moisture);
	const lightLux = Number(record.light);

	if (!Number.isFinite(timestamp.getTime())) return null;
	if (![temp, humidity, moisture, lightLux].every(Number.isFinite)) return null;

	return {
		timestamp,
		temp,
		humidity,
		soil: moisture,
		light: lightLux / 1000,
	};
}

function formatChartLabel(date, rangeKey) {
	if (rangeKey === '1h' || rangeKey === '24h') {
		return date.toLocaleTimeString('vi-VN', { hour: '2-digit', minute: '2-digit' });
	}
	return date.toLocaleString('vi-VN', {
		day: '2-digit',
		month: '2-digit',
		hour: '2-digit',
		minute: '2-digit',
	});
}

function renderForecast(forecast) {
	const row = document.getElementById('forecast-row');
	if (!forecast?.length) {
		row.innerHTML = '<p>Không thể tải dự báo thời tiết.</p>';
		return;
	}

	row.innerHTML = forecast.map(item => `
		<div class="fc-card">
			<span class="fc-day">${item.day}</span>
			<svg class="icon" viewBox="0 0 24 24" style="width:28px;height:28px;color:var(--sun);">${ICONS[item.icon]}</svg>
			<div class="fc-temp"><span>${item.high}°</span><span class="fc-low">${item.low}°</span></div>
			<span class="fc-rain">${item.rain}% mưa</span>
		</div>
	`).join('');
}

function getVietnamDateKey() {
	const parts = new Intl.DateTimeFormat('en', {
		timeZone: 'Asia/Ho_Chi_Minh',
		year: 'numeric',
		month: '2-digit',
		day: '2-digit',
	}).formatToParts(new Date());
	const values = Object.fromEntries(parts.map(part => [part.type, part.value]));
	return `${values.year}-${values.month}-${values.day}`;
}

function buildForecast(payload) {
	const daily = payload.daily;
	if (!daily?.time?.length || !daily.weather_code || !daily.temperature_2m_max || !daily.temperature_2m_min) {
		throw new Error('Dữ liệu dự báo thời tiết không hợp lệ.');
	}

	return daily.time.map((date, index) => {
		const dayDate = new Date(`${date}T12:00:00Z`);
		const high = Number(daily.temperature_2m_max[index]);
		const low = Number(daily.temperature_2m_min[index]);
		const rainValue = Number(daily.precipitation_probability_max?.[index]);
		if (!Number.isFinite(high) || !Number.isFinite(low)) {
			throw new Error('Dữ liệu nhiệt độ dự báo không hợp lệ.');
		}

		return {
			day: index === 0
				? 'Hôm nay'
				: dayDate.toLocaleDateString('vi-VN', { weekday: 'short', timeZone: 'Asia/Ho_Chi_Minh' }),
			icon: WEATHER_ICONS[daily.weather_code[index]] || 'cloud-sun',
			high: Math.round(high),
			low: Math.round(low),
			rain: Number.isFinite(rainValue) ? Math.round(rainValue) : '--',
		};
	});
}

function saveForecastCache(date, items) {
	try {
		localStorage.setItem(FORECAST_CACHE_KEY, JSON.stringify({ date, items }));
	} catch (error) {
		console.warn('Không thể lưu cache dự báo thời tiết:', error);
	}
}

async function loadForecast() {
	const today = getVietnamDateKey();
	try {
		const cached = JSON.parse(localStorage.getItem(FORECAST_CACHE_KEY) || 'null');
		if (cached?.date === today) {
			renderForecast(cached.items);
			return;
		}
	} catch (error) {
		console.warn('Không thể đọc cache dự báo thời tiết:', error);
	}

	const params = new URLSearchParams({
		latitude: '16.0544',
		longitude: '108.2022',
		daily: 'weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max',
		timezone: 'Asia/Ho_Chi_Minh',
		forecast_days: '6',
	});

	try {
		const response = await fetch(`https://api.open-meteo.com/v1/forecast?${params}`);
		if (!response.ok) throw new Error(`HTTP ${response.status}`);

		const forecast = buildForecast(await response.json());
		saveForecastCache(today, forecast);
		renderForecast(forecast);
	} catch (error) {
		saveForecastCache(today, null);
		renderForecast(null);
		console.error('Không thể tải dự báo thời tiết Đà Nẵng:', error);
	}
}

function renderRangeButtons() {
	const group = document.getElementById('range-group');
	group.innerHTML = RANGES.map(range => `
		<button class="range-btn ${range.key === currentRange ? 'active' : ''}" data-range="${range.key}">${range.label}</button>
	`).join('');

	group.querySelectorAll('.range-btn').forEach(button => {
		button.addEventListener('click', () => {
			currentRange = button.dataset.range;
			renderRangeButtons();
			refreshData();
		});
	});
}

function renderMetricChips() {
	const row = document.getElementById('metric-row');
	row.innerHTML = METRICS.map(metric => {
		const active = activeMetrics.includes(metric.key);
		return `
			<button class="metric-chip" data-metric="${metric.key}" style="border-color:${active ? metric.color : '#3A4A34'};background:${active ? `${metric.color}22` : 'transparent'};color:${active ? metric.color : '#8A9280'};">
				<span class="dot" style="background:${metric.color};"></span>${metric.label}
			</button>
		`;
	}).join('');

	row.querySelectorAll('.metric-chip').forEach(chip => {
		chip.addEventListener('click', () => {
			const key = chip.dataset.metric;
			activeMetrics = activeMetrics.includes(key)
				? activeMetrics.filter(metric => metric !== key)
				: [...activeMetrics, key];
			renderMetricChips();
			updateChart(currentRecords);
		});
	});
}

function updateSensorCards(record) {
	document.getElementById('temp-val').textContent = `${record.temp.toFixed(1)}°`;
	document.getElementById('hum-val').textContent = `${record.humidity.toFixed(0)}% độ ẩm`;
	document.getElementById('light-val').textContent = `${record.light.toFixed(2)} klux`;
	document.getElementById('soil-val').textContent = `${record.soil.toFixed(0)}%`;

	const lightPercent = Math.min(1, Math.max(0, record.light / 60));
	const circumference = Math.PI * 70;
	document.getElementById('gauge-arc').setAttribute(
		'stroke-dasharray',
		`${lightPercent * circumference} ${circumference}`,
	);

	const soilPercent = Math.min(1, Math.max(0, record.soil / 100));
	const fillHeight = 64 * soilPercent;
	document.getElementById('soil-fill').setAttribute('y', 78 - fillHeight);
	document.getElementById('soil-fill').setAttribute('height', fillHeight);
}

function updateChart(records) {
	const range = RANGES.find(item => item.key === currentRange);
	const cutoff = Date.now() - range.duration;
	const filteredRecords = records.filter(record => record.timestamp.getTime() >= cutoff);
	const labels = filteredRecords.map(record => formatChartLabel(record.timestamp, currentRange));
	const chartRows = METRICS.map(metric => ({
		...metric,
		key: metric.key,
		values: filteredRecords.map(record => record[metric.key]),
	}));
	const datasets = chartRows
		.filter(metric => activeMetrics.includes(metric.key))
		.map(metric => ({
			label: `${metric.label} (${metric.unit})`,
			data: metric.values,
			borderColor: metric.color,
			backgroundColor: metric.color,
			borderWidth: 2,
			pointRadius: filteredRecords.length > 100 ? 0 : 2,
			tension: 0.35,
		}));

	if (!chart) {
		chart = new Chart(document.getElementById('sensorChart'), {
			type: 'line',
			data: { labels, datasets },
			options: {
				responsive: true,
				maintainAspectRatio: false,
				scales: {
					x: { grid: { display: false }, ticks: { color: '#8A9280', font: { family: "'Be Vietnam Pro'" } } },
					y: { grid: { color: '#2B3A26' }, ticks: { color: '#8A9280', font: { family: "'Be Vietnam Pro'" } } },
				},
				plugins: {
					legend: { labels: { color: '#C9D4BE', font: { family: "'Be Vietnam Pro'" } } },
					tooltip: { backgroundColor: '#F3EFE1', titleColor: '#1D2A17', bodyColor: '#1D2A17' },
				},
			},
		});
	} else {
		chart.data.labels = labels;
		chart.data.datasets = datasets;
		chart.update();
	}
}

function updateClock() {
	const now = new Date();
	const time = now.toLocaleTimeString('vi-VN', { hour: '2-digit', minute: '2-digit' });
	const date = now.toLocaleDateString('vi-VN', { weekday: 'long', day: 'numeric', month: 'long' });
	document.getElementById('time-label').textContent = `${date.charAt(0).toUpperCase()}${date.slice(1)}, ${time}`;
}

async function refreshData() {
	const sequence = ++requestSequence;
	const range = RANGES.find(item => item.key === currentRange);
	const start = new Date(Date.now() - range.duration);
	const params = new URLSearchParams({
		start_time: formatDate(start),
		end_time: formatDate(new Date()),
	});
	const connectionLabel = document.getElementById('conn-label');

	try {
		const response = await fetch(`${API_URL}?${params}`, { cache: 'no-store' });
		const payload = await response.json();
		if (!response.ok || payload.status !== 'success') {
			throw new Error(payload.message || `HTTP ${response.status}`);
		}
		if (sequence !== requestSequence) return;

		const rawRecords = Array.isArray(payload.data)
			? payload.data
			: payload.data ? [payload.data] : [];
		const records = rawRecords
			.map(parseSensorRecord)
			.filter(Boolean)
			.sort((a, b) => a.timestamp - b.timestamp);

		currentRecords = records;
		connectionLabel.textContent = records.length ? 'Đã kết nối' : 'Chưa có dữ liệu';
		if (records.length) updateSensorCards(records[records.length - 1]);
		updateChart(records);
	} catch (error) {
		if (sequence !== requestSequence) return;
		connectionLabel.textContent = 'Mất kết nối API';
		console.error('Không thể tải dữ liệu cảm biến:', error);
	}
}

loadForecast();
renderRangeButtons();
renderMetricChips();
updateClock();
refreshData();
window.setInterval(refreshData, REFRESH_INTERVAL);
window.setInterval(updateClock, 30000);
