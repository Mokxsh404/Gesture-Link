// ========== NAVBAR ==========
const navbar = document.getElementById('navbar');
window.addEventListener('scroll', () => {
  navbar.classList.toggle('scrolled', window.scrollY > 50);
});

// Mobile toggle
const navToggle = document.getElementById('navToggle');
const navLinks = document.getElementById('navLinks');
if (navToggle) {
  navToggle.addEventListener('click', () => {
    navLinks.classList.toggle('open');
    const spans = navToggle.querySelectorAll('span');
    const isOpen = navLinks.classList.contains('open');
    spans[0].style.transform = isOpen ? 'rotate(45deg) translate(5px, 5px)' : 'none';
    spans[1].style.opacity = isOpen ? '0' : '1';
    spans[2].style.transform = isOpen ? 'rotate(-45deg) translate(6px, -6px)' : 'none';
  });
}

// ========== SCROLL REVEAL ==========
const revealElements = document.querySelectorAll('.reveal');
const revealOnScroll = () => {
  const trigger = window.innerHeight * 0.9;
  revealElements.forEach(el => {
    if (el.getBoundingClientRect().top < trigger) el.classList.add('visible');
  });
};
window.addEventListener('scroll', revealOnScroll);
revealOnScroll();

// ========== STATS COUNTER ==========
let statsStarted = false;
const checkStatsInView = () => {
  const section = document.querySelector('.stats-section');
  if (!section || statsStarted) return;
  if (section.getBoundingClientRect().top < window.innerHeight) {
    statsStarted = true;
    document.querySelectorAll('.stat-number').forEach(stat => {
      const target = parseFloat(stat.dataset.target);
      const suffix = stat.dataset.suffix || '';
      const start = performance.now();
      const duration = 2000;
      const animate = (now) => {
        const p = Math.min((now - start) / duration, 1);
        const ease = p * (2 - p);
        stat.textContent = (target % 1 === 0 ? Math.floor(ease * target) : (ease * target).toFixed(1)) + suffix;
        if (p < 1) requestAnimationFrame(animate);
        else stat.textContent = target + suffix;
      };
      requestAnimationFrame(animate);
    });
  }
};
window.addEventListener('scroll', checkStatsInView);
checkStatsInView();

// ========== ANIMATED GROWTH CHART ==========
let chartAnimated = false;

const chartData = {
  vision:  [[60,220],[120,200],[180,160],[240,120],[300,100],[360,90],[420,110],[480,130],[540,150],[560,160]],
  hearing: [[60,260],[120,250],[180,230],[240,190],[300,160],[360,140],[420,150],[480,160],[540,170],[560,175]],
  speech:  [[60,275],[120,265],[180,245],[240,215],[300,190],[360,170],[420,180],[480,190],[540,200],[560,210]]
};

const chartColors = {
  vision:  'rgba(0,212,255,0.8)',
  hearing: 'rgba(139,92,246,0.8)',
  speech:  'rgba(0,184,156,0.8)'
};

const chartLabels = {
  vision:  ['0.8B','1.0B','1.4B','1.8B','2.0B','2.2B','1.9B','1.7B','1.5B','1.4B'],
  hearing: ['0.4B','0.5B','0.7B','1.1B','1.3B','1.5B','1.4B','1.3B','1.2B','1.2B'],
  speech:  ['0.1B','0.2B','0.4B','0.8B','1.0B','1.2B','1.1B','1.0B','0.9B','0.8B']
};

function lerp(a, b, t) { return a + (b - a) * t; }

function buildPointsString(points, progress) {
  const totalPoints = points.length;
  const exactIndex = progress * (totalPoints - 1);
  const visibleCount = Math.floor(exactIndex) + 1;
  const fraction = exactIndex - Math.floor(exactIndex);
  let result = [];
  for (let i = 0; i < Math.min(visibleCount, totalPoints); i++) {
    result.push(`${points[i][0]},${points[i][1]}`);
  }
  if (visibleCount < totalPoints && fraction > 0) {
    const curr = points[visibleCount - 1];
    const next = points[visibleCount];
    const ix = lerp(curr[0], next[0], fraction);
    const iy = lerp(curr[1], next[1], fraction);
    result.push(`${ix.toFixed(1)},${iy.toFixed(1)}`);
  }
  return result;
}

function buildAreaPoints(linePoints) {
  if (linePoints.length === 0) return '60,280';
  const firstX = linePoints[0].split(',')[0];
  const lastX = linePoints[linePoints.length - 1].split(',')[0];
  return `${firstX},280 ${linePoints.join(' ')} ${lastX},280`;
}

function createDots(groupId, points, color) {
  const group = document.getElementById(groupId);
  if (!group) return;
  group.innerHTML = '';
  const ns = 'http://www.w3.org/2000/svg';
  points.forEach((pt, i) => {
    const glow = document.createElementNS(ns, 'circle');
    glow.setAttribute('cx', pt[0]);
    glow.setAttribute('cy', pt[1]);
    glow.setAttribute('r', '8');
    glow.setAttribute('fill', color);
    glow.setAttribute('class', 'chart-dot-glow');
    glow.setAttribute('data-index', i);
    group.appendChild(glow);
    const dot = document.createElementNS(ns, 'circle');
    dot.setAttribute('cx', pt[0]);
    dot.setAttribute('cy', pt[1]);
    dot.setAttribute('r', '4');
    dot.setAttribute('fill', color);
    dot.setAttribute('stroke', '#06060c');
    dot.setAttribute('stroke-width', '2');
    dot.setAttribute('class', 'chart-dot');
    dot.setAttribute('data-index', i);
    group.appendChild(dot);
  });
}

function animateChart() {
  if (chartAnimated) return;
  chartAnimated = true;

  const duration = 2500;
  const stagger = 400;
  const start = performance.now();

  createDots('dotsVision',  chartData.vision,  chartColors.vision);
  createDots('dotsHearing', chartData.hearing, chartColors.hearing);
  createDots('dotsSpeech',  chartData.speech,  chartColors.speech);

  function frame(now) {
    const elapsed = now - start;
    const datasets    = ['vision',     'hearing',     'speech'];
    const lineIds     = ['lineVision', 'lineHearing', 'lineSpeech'];
    const areaIds     = ['areaVision', 'areaHearing', 'areaSpeech'];
    const dotGroupIds = ['dotsVision', 'dotsHearing', 'dotsSpeech'];

    datasets.forEach((key, idx) => {
      const lineElapsed = elapsed - (idx * stagger);
      if (lineElapsed <= 0) return;

      const progress = Math.min(lineElapsed / duration, 1);
      const eased    = 1 - Math.pow(1 - progress, 3);

      const pts   = buildPointsString(chartData[key], eased);
      const lineEl = document.getElementById(lineIds[idx]);
      const areaEl = document.getElementById(areaIds[idx]);
      if (!lineEl || !areaEl) return;

      lineEl.setAttribute('points', pts.join(' '));
      areaEl.setAttribute('points', buildAreaPoints(pts));
      areaEl.setAttribute('opacity', Math.min(eased * 1.5, 0.8).toString());

      const totalPoints    = chartData[key].length;
      const visibleDotCount = Math.floor(eased * totalPoints);
      const dotGroup = document.getElementById(dotGroupIds[idx]);
      if (!dotGroup) return;
      dotGroup.querySelectorAll('.chart-dot').forEach((dot, di) => {
        if (di < visibleDotCount) dot.classList.add('visible');
      });
      dotGroup.querySelectorAll('.chart-dot-glow').forEach((glow, di) => {
        if (di < visibleDotCount) glow.classList.add('visible');
      });
    });

    const totalTime = duration + (stagger * 2);
    if (elapsed < totalTime) {
      requestAnimationFrame(frame);
    } else {
      setupChartTooltips();
    }
  }

  requestAnimationFrame(frame);
}

function setupChartTooltips() {
  const tooltip     = document.getElementById('chartTooltip');
  const tooltipText = document.getElementById('tooltipText');
  if (!tooltip || !tooltipText) return;
  const datasets    = ['vision',     'hearing',     'speech'];
  const dotGroupIds = ['dotsVision', 'dotsHearing', 'dotsSpeech'];

  dotGroupIds.forEach((groupId, dIdx) => {
    const group = document.getElementById(groupId);
    if (!group) return;
    group.querySelectorAll('.chart-dot').forEach(dot => {
      const idx   = parseInt(dot.getAttribute('data-index'));
      const label = chartLabels[datasets[dIdx]][idx];
      const cx    = parseFloat(dot.getAttribute('cx'));
      const cy    = parseFloat(dot.getAttribute('cy'));
      dot.addEventListener('mouseenter', () => {
        tooltipText.textContent = label;
        tooltip.setAttribute('opacity', '1');
        tooltip.setAttribute('transform', `translate(${cx - 45},${cy - 36})`);
        dot.setAttribute('r', '6');
      });
      dot.addEventListener('mouseleave', () => {
        tooltip.setAttribute('opacity', '0');
        dot.setAttribute('r', '4');
      });
    });
  });
}

// Trigger chart animation when the graph container enters the viewport
(function initChartObserver() {
  const graphContainer = document.getElementById('growthChart');
  if (!graphContainer) return;
  if ('IntersectionObserver' in window) {
    const obs = new IntersectionObserver((entries) => {
      entries.forEach(entry => {
        if (entry.isIntersecting) {
          // Small delay so the reveal CSS transition has time to fade the container in
          setTimeout(animateChart, 300);
          obs.disconnect();
        }
      });
    }, { threshold: 0.25 });
    obs.observe(graphContainer);
  } else {
    // Fallback: just animate on scroll
    const fallback = () => {
      const rect = graphContainer.getBoundingClientRect();
      if (rect.top < window.innerHeight * 0.85) {
        animateChart();
        window.removeEventListener('scroll', fallback);
      }
    };
    window.addEventListener('scroll', fallback);
    fallback();
  }
})();

// ========== SKELETON LOADING SYSTEM ==========
(function initSkeletonLoading() {
  const skeletonSVG = `<svg viewBox="0 0 24 24" fill="none" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
    <rect x="3" y="3" width="18" height="18" rx="2" ry="2"/>
    <circle cx="8.5" cy="8.5" r="1.5"/>
    <polyline points="21 15 16 10 5 21"/>
  </svg>`;

  const imageContainers = document.querySelectorAll(
    '.gallery-image-container, .circuit-image-container, .components-main'
  );

  imageContainers.forEach(container => {
    // Guard: skip if skeleton already injected
    if (container.querySelector('.skeleton-loader')) return;

    const img = container.querySelector('img');
    if (!img) return;

    // Ensure container is positioned so absolute children work
    const pos = getComputedStyle(container).position;
    if (pos === 'static') container.style.position = 'relative';

    // Skeleton shimmer overlay
    const skeleton = document.createElement('div');
    skeleton.className = 'skeleton-loader';

    // Skeleton icon (image icon + "Loading" text)
    const icon = document.createElement('div');
    icon.className = 'skeleton-icon';
    icon.innerHTML = skeletonSVG + '<span>Loading</span>';

    container.appendChild(skeleton);
    container.appendChild(icon);

    const onLoad = () => {
      img.classList.add('img-loaded');
      skeleton.classList.add('loaded');
      icon.classList.add('loaded');
    };

    const onError = () => {
      skeleton.classList.add('loaded');
      const span = icon.querySelector('span');
      const svg  = icon.querySelector('svg');
      if (span) span.textContent = 'Image not found';
      if (svg)  svg.style.stroke = '#ec4899';
    };

    if (img.complete && img.naturalWidth > 0) {
      // Already in cache — instant reveal
      onLoad();
    } else {
      img.addEventListener('load',  onLoad,  { once: true });
      img.addEventListener('error', onError, { once: true });
    }
  });
})();


// ========== DEMO TABS ==========
const tabs = document.querySelectorAll('.demo-tab');
const panels = document.querySelectorAll('.demo-panel');

tabs.forEach(tab => {
  tab.addEventListener('click', () => {
    tabs.forEach(t => t.classList.remove('active'));
    panels.forEach(p => p.classList.remove('active'));
    tab.classList.add('active');
    document.getElementById('panel-' + tab.dataset.tab).classList.add('active');
  });
});

// ========== HELPERS ==========
const sleep = ms => new Promise(r => setTimeout(r, ms));
let runningDemo = null;

// ========== GLOVE A DEMO ==========
const gaPlayBtn = document.getElementById('gaPlayBtn');
let gaRunning = false;

const gestures = [
  {
    name: 'Hello',
    phrase: '"Hello, how are you?"',
    fingers: { index: 85, middle: 82, ring: 78, little: 80 },
    imu: { x: 1.2, y: -0.5, z: 9.4 }
  },
  {
    name: 'Thank You',
    phrase: '"Thank you!"',
    fingers: { index: 90, middle: 20, ring: 15, little: 10 },
    imu: { x: -0.8, y: 2.1, z: 9.6 }
  },
  {
    name: 'Help',
    phrase: '"I need help."',
    fingers: { index: 45, middle: 88, ring: 85, little: 90 },
    imu: { x: 3.5, y: -1.2, z: 8.8 }
  }
];

function setFingerBend(finger, percent) {
  const el = document.getElementById('finger-' + finger);
  if (!el) return;
  // percent 0 = straight, 100 = fully bent
  const angle = (percent / 100) * 35;
  el.style.transform = `rotate(${angle}deg)`;
}

function setSensorBar(finger, percent) {
  const bar = document.getElementById('bar-' + finger);
  const val = document.getElementById('val-' + finger);
  if (bar) bar.style.width = percent + '%';
  if (val) val.textContent = Math.round(percent);
}

function setImu(x, y, z) {
  document.getElementById('imu-x').textContent = x.toFixed(1);
  document.getElementById('imu-y').textContent = y.toFixed(1);
  document.getElementById('imu-z').textContent = z.toFixed(1);
}

function resetGloveA() {
  gaRunning = false;
  ['index', 'middle', 'ring', 'little'].forEach(f => {
    setFingerBend(f, 0);
    setSensorBar(f, 0);
  });
  setImu(0.0, 0.0, 9.8);
  document.getElementById('gaBadge').className = 'gesture-badge idle';
  document.getElementById('gaBadge').textContent = 'Idle';
  document.getElementById('gaOutput').classList.remove('visible');
  document.getElementById('gaOutputText').textContent = '';
}

async function runGloveADemo() {
  if (gaRunning) { resetGloveA(); return; }
  gaRunning = true;
  runningDemo = 'glove-a';

  gaPlayBtn.querySelector('svg').style.stroke = '#ff4a4a';
  gaPlayBtn.querySelector('svg').innerHTML = '<rect x="4" y="4" width="16" height="16" fill="currentColor" stroke="none"/>';
  gaPlayBtn.childNodes[gaPlayBtn.childNodes.length - 1].textContent = ' Reset';

  for (let g = 0; g < gestures.length; g++) {
    if (!gaRunning) return;
    const gesture = gestures[g];

    // Reset for new gesture
    document.getElementById('gaOutput').classList.remove('visible');

    // Animate finger bending over 800ms
    const badge = document.getElementById('gaBadge');
    badge.className = 'gesture-badge detecting';
    badge.textContent = 'Detecting...';

    const fingers = ['index', 'middle', 'ring', 'little'];
    const steps = 16;
    for (let s = 1; s <= steps; s++) {
      if (!gaRunning) return;
      fingers.forEach(f => {
        const target = gesture.fingers[f];
        const current = (target / steps) * s;
        setFingerBend(f, current);
        setSensorBar(f, current);
      });
      // Animate IMU gradually
      setImu(
        gesture.imu.x * (s / steps),
        gesture.imu.y * (s / steps),
        9.8 + (gesture.imu.z - 9.8) * (s / steps)
      );
      await sleep(50);
    }

    if (!gaRunning) return;
    await sleep(400);

    // Matched
    badge.className = 'gesture-badge matched';
    badge.textContent = 'Matched: ' + gesture.name;

    // Show speech output
    const output = document.getElementById('gaOutput');
    const outputText = document.getElementById('gaOutputText');
    outputText.textContent = gesture.phrase;
    output.classList.add('visible');

    await sleep(2500);
    if (!gaRunning) return;

    // Unbend fingers
    for (let s = steps; s >= 0; s--) {
      if (!gaRunning) return;
      fingers.forEach(f => {
        const target = gesture.fingers[f];
        const current = (target / steps) * s;
        setFingerBend(f, current);
        setSensorBar(f, current);
      });
      await sleep(30);
    }
    setImu(0.0, 0.0, 9.8);
    output.classList.remove('visible');
    await sleep(600);
  }

  if (gaRunning) {
    await sleep(1000);
    resetGloveA();
    gaPlayBtn.querySelector('svg').style.stroke = 'var(--accent-purple)';
    gaPlayBtn.querySelector('svg').innerHTML = '<polygon points="5 3 19 12 5 21 5 3" fill="currentColor" stroke="none"/>';
    gaPlayBtn.childNodes[gaPlayBtn.childNodes.length - 1].textContent = ' Play Glove A demo';
  }
}

if (gaPlayBtn) gaPlayBtn.addEventListener('click', runGloveADemo);


// ========== VISORA DEMO ==========
const visoraPlayBtn = document.getElementById('visoraPlayBtn');
let visoraRunning = false;

function resetVisora() {
  visoraRunning = false;
  for (let i = 1; i <= 3; i++) {
    document.getElementById('vobj-' + i).classList.remove('visible');
    document.getElementById('vbbox-' + i).classList.remove('detected');
    document.getElementById('vlabel-' + i).classList.remove('visible');
    if (document.getElementById('vswatch-' + i))
      document.getElementById('vswatch-' + i).classList.remove('visible');
  }
  document.getElementById('visoraWave').classList.remove('active');
  document.getElementById('visoraMode').textContent = 'MODE: DETECT';
}

async function runVisoraDemo() {
  if (visoraRunning) { resetVisora(); return; }
  visoraRunning = true;
  runningDemo = 'visora';

  visoraPlayBtn.querySelector('svg').style.stroke = '#ff4a4a';
  visoraPlayBtn.querySelector('svg').innerHTML = '<rect x="4" y="4" width="16" height="16" fill="currentColor" stroke="none"/>';
  visoraPlayBtn.childNodes[visoraPlayBtn.childNodes.length - 1].textContent = ' Reset';

  resetVisora();
  visoraRunning = true;

  // Step 1: Object detection mode
  document.getElementById('visoraMode').textContent = 'MODE: DETECT';
  await sleep(600);

  // Object 1 appears
  if (!visoraRunning) return;
  document.getElementById('vobj-1').classList.add('visible');
  await sleep(500);
  document.getElementById('vbbox-1').classList.add('detected');
  await sleep(300);
  document.getElementById('vlabel-1').classList.add('visible');

  // Audio speaks
  document.getElementById('visoraWave').classList.add('active');
  await sleep(1500);
  document.getElementById('visoraWave').classList.remove('active');

  if (!visoraRunning) return;

  // Object 2
  document.getElementById('vobj-2').classList.add('visible');
  await sleep(500);
  document.getElementById('vbbox-2').classList.add('detected');
  await sleep(300);
  document.getElementById('vlabel-2').classList.add('visible');

  document.getElementById('visoraWave').classList.add('active');
  await sleep(1500);
  document.getElementById('visoraWave').classList.remove('active');

  if (!visoraRunning) return;

  // Step 2: Switch to colour mode
  await sleep(800);
  document.getElementById('visoraMode').textContent = 'MODE: COLOUR';

  // Object 3 (colour)
  document.getElementById('vobj-3').classList.add('visible');
  await sleep(500);
  document.getElementById('vbbox-3').classList.add('detected');
  document.getElementById('vbbox-3').style.borderColor = 'var(--accent-pink)';
  document.getElementById('vbbox-3').style.boxShadow = '0 0 16px rgba(236,72,153,0.2)';
  await sleep(300);
  document.getElementById('vlabel-3').classList.add('visible');

  if (!visoraRunning) return;

  // Show colour swatches
  for (let i = 1; i <= 3; i++) {
    await sleep(400);
    if (!visoraRunning) return;
    document.getElementById('vswatch-' + i).classList.add('visible');
  }

  // Audio
  document.getElementById('visoraWave').classList.add('active');
  await sleep(2000);
  document.getElementById('visoraWave').classList.remove('active');

  // Hold and reset
  await sleep(3000);
  if (visoraRunning) {
    resetVisora();
    visoraPlayBtn.querySelector('svg').style.stroke = 'var(--accent-cyan)';
    visoraPlayBtn.querySelector('svg').innerHTML = '<polygon points="5 3 19 12 5 21 5 3" fill="currentColor" stroke="none"/>';
    visoraPlayBtn.childNodes[visoraPlayBtn.childNodes.length - 1].textContent = ' Play Visora demo';
  }
}

if (visoraPlayBtn) visoraPlayBtn.addEventListener('click', runVisoraDemo);


// ========== GLOVE B DEMO (LIVE SPEECH-TO-BRAILLE) ==========
const gbPlayBtn = document.getElementById('gbPlayBtn');
let gbRunning = false;
let gbRecognition = null;

// Full Braille alphabet — standard 6-dot cell
// Dot positions: 1(top-left), 2(mid-left), 3(bot-left), 4(top-right), 5(mid-right), 6(bot-right)
const brailleAlphabet = {
  'A': [1],        'B': [1, 2],      'C': [1, 4],      'D': [1, 4, 5],
  'E': [1, 5],     'F': [1, 2, 4],   'G': [1, 2, 4, 5],'H': [1, 2, 5],
  'I': [2, 4],     'J': [2, 4, 5],   'K': [1, 3],      'L': [1, 2, 3],
  'M': [1, 3, 4],  'N': [1, 3, 4, 5],'O': [1, 3, 5],   'P': [1, 2, 3, 4],
  'Q': [1, 2, 3, 4, 5], 'R': [1, 2, 3, 5], 'S': [2, 3, 4], 'T': [2, 3, 4, 5],
  'U': [1, 3, 6],  'V': [1, 2, 3, 6],'W': [2, 4, 5, 6],'X': [1, 3, 4, 6],
  'Y': [1, 3, 4, 5, 6], 'Z': [1, 3, 5, 6],
  '0': [2, 4, 5, 6], '1': [1], '2': [1, 2], '3': [1, 4], '4': [1, 4, 5],
  '5': [1, 5], '6': [1, 2, 4], '7': [1, 2, 4, 5], '8': [1, 2, 5], '9': [2, 4],
  ' ': []
};

// Check for browser speech recognition support
const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;

function resetGloveB() {
  gbRunning = false;
  // Stop any active recognition
  if (gbRecognition) {
    try { gbRecognition.abort(); } catch(e) {}
    gbRecognition = null;
  }
  document.getElementById('gbMic').classList.remove('recording');
  document.getElementById('gbMicStatus').className = 'mic-status';
  document.getElementById('gbMicStatus').textContent = 'Tap to speak';
  document.getElementById('gbPlaceholder').style.display = '';
  document.getElementById('gbPlaceholder').textContent = 'Waiting for speech...';
  document.getElementById('gbTextContent').classList.remove('visible');
  document.getElementById('gbTextContent').textContent = '';
  document.getElementById('gbArrow1').classList.remove('active');
  document.getElementById('gbArrow2').classList.remove('active');
  document.getElementById('gbBrailleOutput').innerHTML = '';
  document.getElementById('gbMotorGrid').classList.remove('visible');
  document.getElementById('gbMotorStatus').textContent = 'Braille output';
  document.getElementById('gbMotorStatus').className = 'mic-status';
  for (let i = 0; i < 12; i++) {
    document.getElementById('motor-' + i).classList.remove('vibrating');
  }
  const cell1El = document.getElementById('gb-motor-cell-1');
  const cell2El = document.getElementById('gb-motor-cell-2');
  if (cell1El) cell1El.classList.remove('active-cell');
  if (cell2El) cell2El.classList.remove('active-cell');

  // Reset button
  gbPlayBtn.querySelector('svg').style.stroke = 'var(--accent-teal)';
  gbPlayBtn.querySelector('svg').innerHTML = '<polygon points="5 3 19 12 5 21 5 3" fill="currentColor" stroke="none"/>';
  const lastNode = gbPlayBtn.childNodes[gbPlayBtn.childNodes.length - 1];
  if (lastNode) lastNode.textContent = ' Speak & convert to Braille';
}

async function renderBrailleForText(text) {
  const chars = text.toUpperCase().split('');
  const brailleOutput = document.getElementById('gbBrailleOutput');
  brailleOutput.innerHTML = '';

  document.getElementById('gbArrow2').classList.add('active');
  document.getElementById('gbMotorGrid').classList.add('visible');
  document.getElementById('gbMotorStatus').className = 'mic-status active';
  document.getElementById('gbMotorStatus').textContent = 'Vibrating...';

  const cell1El = document.getElementById('gb-motor-cell-1');
  const cell2El = document.getElementById('gb-motor-cell-2');
  if (cell1El) cell1El.classList.remove('active-cell');
  if (cell2El) cell2El.classList.remove('active-cell');

  await sleep(400);

  let useCell2 = false; // alternates between Cell 1 (Index/Middle) and Cell 2 (Ring/Little)

  for (const char of chars) {
    if (!gbRunning) return;

    const activeDots = brailleAlphabet[char];
    if (activeDots === undefined) continue; // skip unsupported chars

    // Build braille cell UI
    const wrapper = document.createElement('div');
    const cell = document.createElement('div');
    cell.className = 'braille-cell';
    for (let d = 1; d <= 6; d++) {
      const dot = document.createElement('div');
      dot.className = 'braille-dot' + (activeDots.includes(d) ? ' active' : '');
      cell.appendChild(dot);
    }
    const label = document.createElement('div');
    label.className = 'braille-char-label';
    label.textContent = char === ' ' ? '␣' : char;
    wrapper.appendChild(cell);
    wrapper.appendChild(label);
    brailleOutput.appendChild(wrapper);
    requestAnimationFrame(() => cell.classList.add('visible'));

    // Check if character is a space/empty (no vibration or highlight toggle)
    const isSpace = activeDots.length === 0;

    if (!isSpace) {
      const motorOffset = useCell2 ? 6 : 0;

      // Highlight the active motor cell
      if (cell1El && cell2El) {
        cell1El.classList.toggle('active-cell', !useCell2);
        cell2El.classList.toggle('active-cell', useCell2);
      }

      // Activate motors for this character
      activeDots.forEach(d => {
        const motor = document.getElementById('motor-' + (d - 1 + motorOffset));
        if (motor) motor.classList.add('vibrating');
      });

      // Vibrate mobile device if API is available
      if (navigator.vibrate) {
        navigator.vibrate(200);
      }

      await sleep(600);

      // Reset motors & active highlights
      for (let i = 0; i < 12; i++) {
        document.getElementById('motor-' + i).classList.remove('vibrating');
      }
      if (cell1El) cell1El.classList.remove('active-cell');
      if (cell2El) cell2El.classList.remove('active-cell');

      // Toggle cell for the next character
      useCell2 = !useCell2;
    } else {
      // For spaces, pause briefly without vibration
      await sleep(600);
    }

    await sleep(150);
  }

  document.getElementById('gbMotorStatus').textContent = 'Complete';
}

async function runGloveBDemo() {
  // If already running, reset
  if (gbRunning) { resetGloveB(); return; }

  // Check browser support
  if (!SpeechRecognition) {
    document.getElementById('gbPlaceholder').textContent = 'Speech recognition not supported in this browser. Try Chrome.';
    return;
  }

  gbRunning = true;
  runningDemo = 'glove-b';

  // Update button to reset mode
  gbPlayBtn.querySelector('svg').style.stroke = '#ff4a4a';
  gbPlayBtn.querySelector('svg').innerHTML = '<rect x="4" y="4" width="16" height="16" fill="currentColor" stroke="none"/>';
  const lastNode = gbPlayBtn.childNodes[gbPlayBtn.childNodes.length - 1];
  if (lastNode) lastNode.textContent = ' Reset';

  // Clear previous results
  document.getElementById('gbBrailleOutput').innerHTML = '';
  document.getElementById('gbTextContent').classList.remove('visible');
  document.getElementById('gbTextContent').textContent = '';
  document.getElementById('gbPlaceholder').style.display = '';
  document.getElementById('gbPlaceholder').textContent = 'Listening...';
  document.getElementById('gbArrow1').classList.remove('active');
  document.getElementById('gbArrow2').classList.remove('active');
  document.getElementById('gbMotorGrid').classList.remove('visible');

  // Start recording
  document.getElementById('gbMic').classList.add('recording');
  document.getElementById('gbMicStatus').className = 'mic-status active';
  document.getElementById('gbMicStatus').textContent = 'Listening...';

  // Create speech recognition instance
  gbRecognition = new SpeechRecognition();
  gbRecognition.lang = 'en-US';
  gbRecognition.interimResults = true;
  gbRecognition.maxAlternatives = 1;
  gbRecognition.continuous = false;

  gbRecognition.onresult = async (event) => {
    const result = event.results[0];
    const transcript = result[0].transcript;

    // Show interim results live
    document.getElementById('gbPlaceholder').style.display = 'none';
    const textEl = document.getElementById('gbTextContent');
    textEl.textContent = transcript.toUpperCase();
    textEl.classList.add('visible');

    if (result.isFinal) {
      // Final result — stop recording and convert to braille
      document.getElementById('gbMic').classList.remove('recording');
      document.getElementById('gbMicStatus').textContent = 'Processing...';
      document.getElementById('gbArrow1').classList.add('active');

      await sleep(600);
      if (!gbRunning) return;

      document.getElementById('gbMicStatus').textContent = 'Done';

      await sleep(400);
      if (!gbRunning) return;

      // Convert to Braille
      await renderBrailleForText(transcript);
    }
  };

  gbRecognition.onerror = (event) => {
    console.error('Speech recognition error:', event.error);
    document.getElementById('gbMic').classList.remove('recording');
    if (event.error === 'no-speech') {
      document.getElementById('gbMicStatus').textContent = 'No speech detected';
      document.getElementById('gbPlaceholder').textContent = 'No speech detected. Try again.';
    } else if (event.error === 'not-allowed') {
      document.getElementById('gbMicStatus').textContent = 'Mic blocked';
      document.getElementById('gbPlaceholder').textContent = 'Microphone access denied. Allow mic and try again.';
    } else if (event.error === 'network') {
      document.getElementById('gbMicStatus').textContent = 'Network error';
      document.getElementById('gbPlaceholder').textContent = 'Network error: Browser speech recognition requires an active internet connection to contact voice servers.';
    } else {
      document.getElementById('gbMicStatus').textContent = 'Error';
      document.getElementById('gbPlaceholder').textContent = 'Error: ' + event.error;
    }
  };

  gbRecognition.onend = () => {
    // Recognition ended (could be after result or after error)
    document.getElementById('gbMic').classList.remove('recording');
  };

  // Start listening
  try {
    gbRecognition.start();
  } catch (e) {
    document.getElementById('gbMicStatus').textContent = 'Error starting mic';
    document.getElementById('gbPlaceholder').textContent = 'Could not start speech recognition.';
    gbRunning = false;
  }
}

if (gbPlayBtn) gbPlayBtn.addEventListener('click', runGloveBDemo);
