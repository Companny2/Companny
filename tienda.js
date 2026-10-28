/* Tienda PanaRepara — catálogo con búsqueda, filtros y paginación
   (escritorio: 8 por página = 4x2 · móvil: 6 por página = 2x3) */
(function () {
  const GRID   = document.getElementById('tiendaGrid');
  if (!GRID) return;
  const BUSCAR = document.getElementById('tiendaBuscar');
  const CATS   = document.getElementById('tiendaCats');
  const PAG    = document.getElementById('tiendaPag');
  const VACIO  = document.getElementById('tiendaVacio');
  const SEC    = document.getElementById('tienda');
  const WA     = '50760322222';
  const mqMovil = window.matchMedia('(max-width: 768px)');

  let todos = [], filtrados = [], pagina = 1, cat = 'Todas', q = '';

  const esc = s => String(s).replace(/[&<>"']/g, c =>
    ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  // minúsculas + sin tildes, para buscar sin importar acentos
  const norm = s => String(s).toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '');
  const porPagina = () => mqMovil.matches ? 6 : 8;
  const totalPaginas = () => Math.max(1, Math.ceil(filtrados.length / porPagina()));

  fetch('productos.json?v=2')
    .then(r => r.json())
    .then(data => { todos = data; construirCats(); aplicar(); })
    .catch(() => { GRID.innerHTML = '<p class="tienda-vacio">No se pudo cargar el catálogo. Escríbenos por WhatsApp.</p>'; });

  function construirCats() {
    const cats = ['Todas', ...Array.from(new Set(todos.map(p => p.cat))).sort()];
    CATS.innerHTML = cats.map(c =>
      `<button type="button" class="tienda-chip${c === 'Todas' ? ' activo' : ''}" data-cat="${esc(c)}">${esc(c)}</button>`
    ).join('');
    CATS.addEventListener('click', e => {
      const b = e.target.closest('.tienda-chip');
      if (!b) return;
      cat = b.dataset.cat;
      CATS.querySelectorAll('.tienda-chip').forEach(x => x.classList.toggle('activo', x === b));
      aplicar();
    });
  }

  function aplicar() {
    // divide la búsqueda en palabras: deben estar TODAS (en cualquier orden)
    const toks = norm(q).split(/\s+/).filter(Boolean);
    filtrados = todos.filter(p => {
      const okCat = cat === 'Todas' || p.cat === cat;
      if (!okCat) return false;
      if (!toks.length) return true;
      const hay = norm(p.nombre + ' ' + (p.codigo || '') + ' ' + p.cat);
      return toks.every(t => hay.includes(t));
    });
    pagina = 1;
    VACIO.hidden = filtrados.length > 0;
    render();
  }

  function render() {
    const pp = porPagina();
    const tp = totalPaginas();
    if (pagina > tp) pagina = tp;
    const ini = (pagina - 1) * pp;
    const items = filtrados.slice(ini, ini + pp);
    const frag = document.createDocumentFragment();
    items.forEach(p => frag.appendChild(tarjeta(p)));
    GRID.innerHTML = '';
    GRID.appendChild(frag);
    renderPaginacion(tp);
  }

  function irA(p, scroll) {
    pagina = p;
    render();
    if (scroll !== false && SEC) SEC.scrollIntoView({ behavior: 'smooth', block: 'start' });
  }

  function renderPaginacion(tp) {
    PAG.innerHTML = '';
    if (tp <= 1) return;

    const btn = (txt, destino, extra, label) => {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'pag-btn' + (extra ? ' ' + extra : '');
      b.textContent = txt;
      if (label) b.setAttribute('aria-label', label);
      if (destino) b.addEventListener('click', () => irA(destino));
      else b.disabled = true;
      return b;
    };

    PAG.appendChild(btn('‹', pagina > 1 ? pagina - 1 : null, 'pag-nav', 'Página anterior'));

    const nums = [...new Set([1, pagina - 1, pagina, pagina + 1, tp])].filter(n => n >= 1 && n <= tp).sort((a, b) => a - b);
    let last = 0;
    nums.forEach(n => {
      if (n - last > 1) {
        const d = document.createElement('span');
        d.className = 'pag-dots';
        d.textContent = '…';
        PAG.appendChild(d);
      }
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'pag-num' + (n === pagina ? ' activo' : '');
      b.textContent = n;
      b.setAttribute('aria-label', 'Página ' + n);
      if (n === pagina) b.setAttribute('aria-current', 'page');
      b.addEventListener('click', () => irA(n));
      PAG.appendChild(b);
      last = n;
    });

    PAG.appendChild(btn('›', pagina < tp ? pagina + 1 : null, 'pag-nav', 'Página siguiente'));

    const info = document.createElement('span');
    info.className = 'pag-info';
    info.textContent = 'Página ' + pagina + ' de ' + tp + ' · ' + filtrados.length + ' productos';
    PAG.appendChild(info);
  }

  function tarjeta(p) {
    const precio = Number(p.precio).toFixed(2);
    const msg = encodeURIComponent(
      'Hola PanaRepara, me interesa comprar:\n' +
      p.nombre + '\n' +
      'Código: ' + (p.codigo || '') + '\n' +
      'Precio: B/.' + precio + '\n' +
      '¿Está disponible?'
    );
    const el = document.createElement('article');
    el.className = 'tienda-card';
    el.innerHTML =
      '<div class="tienda-img"><img src="' + p.img + '" alt="' + esc(p.nombre) + '" loading="lazy" decoding="async"></div>' +
      '<div class="tienda-info">' +
        '<span class="tienda-cat">' + esc(p.cat) + '</span>' +
        '<h3 class="tienda-nombre">' + esc(p.nombre) + '</h3>' +
        '<div class="tienda-precio">B/. ' + precio + '</div>' +
        '<a class="tienda-comprar" href="https://wa.me/' + WA + '?text=' + msg + '" target="_blank" rel="noopener noreferrer">Comprar por WhatsApp</a>' +
      '</div>';
    return el;
  }

  let t;
  BUSCAR.addEventListener('input', () => {
    clearTimeout(t);
    t = setTimeout(() => { q = BUSCAR.value; aplicar(); }, 180);
  });

  // Recalcular por página al cambiar entre móvil/escritorio
  const onBP = () => { if (filtrados.length) render(); };
  if (mqMovil.addEventListener) mqMovil.addEventListener('change', onBP);
  else if (mqMovil.addListener) mqMovil.addListener(onBP);

  let rt;
  window.addEventListener('resize', () => {
    clearTimeout(rt);
    rt = setTimeout(() => { if (filtrados.length) render(); }, 160);
  });
})();
