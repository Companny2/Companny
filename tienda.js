/* Tienda PanaRepara — carga productos.json, filtra/busca y arma el mensaje de compra por WhatsApp */
(function () {
  const GRID = document.getElementById('tiendaGrid');
  if (!GRID) return;

  const BUSCAR = document.getElementById('tiendaBuscar');
  const CATS   = document.getElementById('tiendaCats');
  const MAS    = document.getElementById('tiendaMas');
  const VACIO  = document.getElementById('tiendaVacio');
  const WA     = '50760322222';
  const LOTE   = 24;

  let todos = [], filtrados = [], mostrados = 0, cat = 'Todas', q = '';

  const esc = s => String(s).replace(/[&<>"']/g, c =>
    ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

  fetch('productos.json?v=1')
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
    const term = q.trim().toLowerCase();
    filtrados = todos.filter(p => {
      const okCat = cat === 'Todas' || p.cat === cat;
      const okQ = !term ||
        p.nombre.toLowerCase().includes(term) ||
        (p.codigo || '').toLowerCase().includes(term) ||
        p.cat.toLowerCase().includes(term);
      return okCat && okQ;
    });
    GRID.innerHTML = '';
    mostrados = 0;
    VACIO.hidden = filtrados.length > 0;
    render();
  }

  function render() {
    const frag = document.createDocumentFragment();
    const fin = Math.min(mostrados + LOTE, filtrados.length);
    for (let i = mostrados; i < fin; i++) frag.appendChild(tarjeta(filtrados[i]));
    GRID.appendChild(frag);
    mostrados = fin;
    MAS.hidden = mostrados >= filtrados.length;
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
  MAS.addEventListener('click', render);
})();
