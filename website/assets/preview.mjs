const previewStates = {
  music: { kicker: '正在播放', title: '属于你的节奏', icon: '#i-headphones' },
  voice: { kicker: '空间语音', title: '和朋友，保持同频', icon: '#i-headphones' },
  run: { kicker: '跑图计时', title: '00:18.420 · 继续向前', icon: '#i-motion' },
};

export function initializePreview() {
  const preview = document.querySelector('#hero-preview');
  const choices = [...document.querySelectorAll('[data-preview]')];
  const controls = document.querySelector('.preview-controls');
  const island = document.querySelector('#dynamic-island');
  const indicator = document.createElement('span');
  indicator.className = 'tab-indicator';
  indicator.setAttribute('aria-hidden', 'true');
  controls.prepend(indicator);
  controls.classList.add('has-indicator');
  controls.hidden = false;
  island.setAttribute('role', 'status');
  island.setAttribute('aria-live', 'polite');
  let activeButton = choices[0];
  let animation;

  function moveIndicator() {
    indicator.style.width = `${activeButton.offsetWidth}px`;
    indicator.style.height = `${activeButton.offsetHeight}px`;
    indicator.style.transform = `translate(${activeButton.offsetLeft}px, ${activeButton.offsetTop}px)`;
  }

  choices.forEach(button => button.addEventListener('click', () => {
    const state = previewStates[button.dataset.preview];
    if (activeButton === button) return;
    activeButton = button;
    preview.dataset.mode = button.dataset.preview;
    choices.forEach(choice => {
      choice.classList.toggle('is-active', choice === button);
      choice.setAttribute('aria-pressed', String(choice === button));
    });
    document.querySelector('#island-kicker').textContent = state.kicker;
    document.querySelector('#island-title').textContent = state.title;
    document.querySelector('#island-icon-use').setAttribute('href', state.icon);
    animation?.cancel();
    if (document.documentElement.dataset.motion === 'on' && island.animate) {
      animation = island.querySelector('.island-text').animate([
        { opacity: 0, transform: 'translateY(5px)', filter: 'blur(3px)' },
        { opacity: 1, transform: 'translateY(0)', filter: 'blur(0)' },
      ], { duration: 400, easing: 'cubic-bezier(.22,1,.36,1)' });
    }
    moveIndicator();
  }));
  moveIndicator();
  if ('ResizeObserver' in window) new ResizeObserver(moveIndicator).observe(controls);
  else window.addEventListener('resize', moveIndicator);

  const settings = document.querySelector('#settings-demo');
  const swatches = [...document.querySelectorAll('[data-color]')];
  swatches.forEach(button => button.addEventListener('click', () => {
    settings.dataset.accent = button.dataset.color;
    swatches.forEach(swatch => swatch.setAttribute('aria-pressed', String(swatch === button)));
  }));
  const cleanToggle = document.querySelector('#clean-toggle');
  cleanToggle.addEventListener('click', () => {
    const enabled = cleanToggle.getAttribute('aria-checked') !== 'true';
    cleanToggle.setAttribute('aria-checked', String(enabled));
    settings.dataset.clean = String(enabled);
  });
}
