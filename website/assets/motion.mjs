import { motionAllowed, readPreference, savePreference } from './preferences.mjs?v=1.0.0';

export function initializeMotion() {
  const root = document.documentElement;
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)');
  const finePointer = window.matchMedia('(hover: hover) and (pointer: fine)');
  const toggle = document.querySelector('#motion-toggle');
  let preference = readPreference('qm-site-motion');
  let observer;

  function applyMotion() {
    const enabled = motionAllowed(reduced.matches, preference);
    root.dataset.motion = enabled ? 'on' : 'off';
    toggle.setAttribute('aria-pressed', String(enabled));
    toggle.querySelector('span').textContent = reduced.matches ? '跟随系统：减少动效' : enabled ? '动效已开启' : '动效已关闭';
    toggle.disabled = reduced.matches;
    toggle.title = reduced.matches ? '系统已启用减少动态效果' : '开启或关闭网页动效';
    if (!enabled) {
      observer?.disconnect();
      document.querySelectorAll('.reveal-pending').forEach(element => element.classList.remove('reveal-pending'));
      document.querySelectorAll('.magnetic').forEach(element => element.style.removeProperty('transform'));
    }
  }

  applyMotion();
  toggle.hidden = false;
  toggle.addEventListener('click', () => {
    preference = root.dataset.motion === 'on' ? 'off' : 'on';
    savePreference('qm-site-motion', preference);
    applyMotion();
  });
  reduced.addEventListener('change', applyMotion);

  // 已在首屏内的内容立即可见，其余区块仅在第一次进入视口时显现。
  if ('IntersectionObserver' in window && root.dataset.motion === 'on') {
    observer = new IntersectionObserver(entries => {
      entries.forEach(entry => {
        if (!entry.isIntersecting) return;
        entry.target.classList.remove('reveal-pending');
        observer.unobserve(entry.target);
      });
    }, { threshold: 0.08, rootMargin: '0px 0px -20px 0px' });
    document.querySelectorAll('.reveal').forEach(element => {
      if (element.getBoundingClientRect().top < window.innerHeight) return;
      element.classList.add('reveal-pending');
      observer.observe(element);
    });
  }
  root.classList.add('motion-ready');

  // 触屏和减少动态效果模式不启用指针跟随。
  document.querySelectorAll('.magnetic').forEach(element => {
    element.addEventListener('pointermove', event => {
      if (root.dataset.motion !== 'on' || !finePointer.matches || event.pointerType === 'touch') return;
      const bounds = element.getBoundingClientRect();
      const x = (event.clientX - bounds.left - bounds.width / 2) * 0.1;
      const y = (event.clientY - bounds.top - bounds.height / 2) * 0.15;
      element.style.transform = `translate(${x}px, ${y}px)`;
    });
    element.addEventListener('pointerleave', () => element.style.removeProperty('transform'));
    element.addEventListener('blur', () => element.style.removeProperty('transform'));
  });
}
