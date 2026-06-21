document.addEventListener('DOMContentLoaded', () => {
  const tarjetas = document.querySelectorAll('.equipo-card');

  const panelServicio = document.getElementById('panelServicio');
  const panelDefault = document.getElementById('panelDefault');
  const panelDetalle = document.getElementById('panelDetalle');

  const descripcionDetalle = document.getElementById('descripcionDetalle');
  const casosReales = document.getElementById('casosReales');

  const marcaEquipo = document.getElementById('marcaEquipo');
  const modeloEquipo = document.getElementById('modeloEquipo');
  const fallaEquipo = document.getElementById('fallaEquipo');

  const btnWhatsapp = document.getElementById('btnWhatsapp');
  const btnVolver = document.getElementById('btnVolver');

  const telefonoWhatsApp = '50760322222';

  let equipoActivo = '';

  const dataServicios = {
    televisores: {
      nombre: 'Televisores',
      titulo: 'Reparación de televisores Samsung y LG en El Dorado, Ciudad de Panamá',
      descripcion: 'Diagnóstico y reparación de televisores Samsung y LG (LED y Smart TV) con fallas de imagen, retroiluminación, audio, fuente de poder y tarjeta principal. Atendemos en Centro Comercial El Dorado, Torre Norte, Nivel Amarillo, local 52A.',
      marcas: ['Samsung', 'LG'],
      ejemploModelo: 'Ejemplo: Samsung UN55TU7000 / LG 50UP7750',
      ejemploFalla: 'Ejemplo: no da imagen, pantalla oscura, no enciende, líneas en la pantalla, sin sonido, etc.',
      casos: [
        { titulo: 'TV sin imagen', texto: 'Revisión de retroiluminación, tiras LED, driver y fuente de poder.' },
        { titulo: 'TV no enciende', texto: 'Diagnóstico de standby, fusibles, fuente primaria y componentes dañados.' },
        { titulo: 'TV con sonido pero sin video', texto: 'Pruebas en panel, T-CON, backlight y tarjeta principal.' }
      ]
    },

    /* SERVICIO SUSPENDIDO (2026-06-19): Videojuegos. Para reactivar, descomentar este bloque
       y la tarjeta correspondiente en index.html.
    videojuegos: {
      nombre: 'Video Juegos',
      titulo: 'Reparación de consolas en El Dorado, Ciudad de Panamá',
      descripcion: 'Servicio técnico para PlayStation, Xbox y Nintendo con fallas de HDMI, sobrecalentamiento, encendido, almacenamiento y mantenimiento interno.',
      marcas: ['PS5', 'PS4', 'Xbox Series', 'Xbox One', 'Nintendo Switch'],
      casos: [
        { titulo: 'Puerto HDMI dañado', texto: 'Cambio de HDMI en consolas sin señal o con imagen intermitente.' },
        { titulo: 'Consola se apaga', texto: 'Mantenimiento interno, limpieza y revisión térmica.' },
        { titulo: 'Sobrecalentamiento', texto: 'Cambio de pasta térmica y limpieza profunda.' }
      ]
    },
    */

    laptops: {
      nombre: 'Laptops',
      titulo: 'Reparación de laptops en El Dorado, Ciudad de Panamá',
      descripcion: 'Diagnóstico y reparación de laptops y laptops gamer (Dell, HP, Asus, Lenovo y MacBook) con fallas de encendido, video, carga, teclado, batería, bisagras y sobrecalentamiento.',
      marcas: ['Dell', 'HP', 'Asus', 'Lenovo', 'MacBook'],
      ejemploModelo: 'Ejemplo: HP Pavilion 15 / MacBook Air 2017',
      ejemploFalla: 'Ejemplo: no enciende, no carga, pantalla negra, se sobrecalienta, falla el teclado, lenta, etc.',
      casos: [
        { titulo: 'Laptop no enciende', texto: 'Revisión de entrada DC, circuito de carga y etapas primarias.' },
        { titulo: 'Laptop sin video', texto: 'Diagnóstico de memoria, BIOS, pantalla, flex y video.' },
        { titulo: 'Sobrecalentamiento', texto: 'Mantenimiento preventivo, limpieza y cambio de pasta térmica.' }
      ]
    },

    impresoras: {
      nombre: 'Impresoras',
      titulo: 'Reparación de impresoras en El Dorado, Ciudad de Panamá',
      descripcion: 'Servicio técnico especializado en impresoras Epson, HP y Canon. Realizamos cambio de cabezales, limpieza profunda, solución de errores de tinta, mantenimiento y reseteo de almohadillas.',
      marcas: ['Epson', 'HP', 'Canon'],
      ejemploModelo: 'Ejemplo: Epson L3250 / Canon G2110',
      ejemploFalla: 'Ejemplo: no imprime negro, presenta líneas, no reconoce cartuchos, error de almohadillas, no enciende, etc.',
      casos: [
        { titulo: 'Cambio de cabezal', texto: 'Reemplazo y prueba de cabezal en impresoras con líneas o falta de color.' },
        { titulo: 'Limpieza profunda', texto: 'Limpieza interna, estación de servicio, sistema de tinta y mantenimiento.' },
        { titulo: 'Error de almohadillas', texto: 'Diagnóstico, mantenimiento y reseteo en impresoras Epson.' }
      ]
    },

    bocinas: {
      nombre: 'Bocinas Bose',
      titulo: 'Reparación de bocinas Bose en El Dorado, Ciudad de Panamá',
      descripcion: 'Diagnóstico y reparación de bocinas portátiles Bose con fallas de carga, batería, Bluetooth, distorsión de audio y conectores dañados.',
      marcas: ['Bose'],
      ejemploModelo: 'Ejemplo: Bose SoundLink Flex / Bose S1 Pro',
      ejemploFalla: 'Ejemplo: no carga, no enciende, audio distorsionado, Bluetooth inestable, sin sonido, etc.',
      casos: [
        { titulo: 'Bocina no carga', texto: 'Revisión de batería, puerto de carga y circuito de alimentación.' },
        { titulo: 'Audio distorsionado', texto: 'Diagnóstico de parlantes, amplificación y sistema interno.' },
        { titulo: 'Bluetooth inestable', texto: 'Revisión del módulo Bluetooth y alimentación.' }
      ]
    }
  };

  function limpiarFormulario() {
    marcaEquipo.value = '';
    modeloEquipo.value = '';
    fallaEquipo.value = '';
  }

  function cargarMarcas(marcas) {
    marcaEquipo.innerHTML = '<option value="">Selecciona una marca</option>';

    marcas.forEach(marca => {
      const option = document.createElement('option');
      option.value = marca;
      option.textContent = marca;
      marcaEquipo.appendChild(option);
    });
  }

  function cargarCasos(casos) {
    casosReales.innerHTML = '';

    casos.forEach(caso => {
      const card = document.createElement('button');
      card.type = 'button';
      card.className = 'caso-card';
      card.innerHTML = `
        <h4>${caso.titulo}</h4>
        <p>${caso.texto}</p>
        <span class="caso-usar">Usar en mi mensaje →</span>
      `;
      card.addEventListener('click', () => usarCaso(caso));
      casosReales.appendChild(card);
    });
  }

  function usarCaso(caso) {
    // Precarga el caso en el resumen de la falla para enviarlo sin escribir
    fallaEquipo.value = caso.titulo;

    const y = panelServicio.getBoundingClientRect().top + window.pageYOffset - 95;
    window.scrollTo({ top: y, behavior: 'smooth' });

    fallaEquipo.classList.add('campo-resaltado');
    setTimeout(() => fallaEquipo.classList.remove('campo-resaltado'), 1400);
  }

  function activarSoloUnaTarjeta(tarjetaActiva) {
    tarjetas.forEach(tarjeta => {
      tarjeta.classList.remove('activo');

      if (tarjeta === tarjetaActiva) {
        tarjeta.classList.add('activo');
      } else {
        tarjeta.classList.add('oculto');
      }
    });
  }

  function mostrarTodasLasTarjetas() {
    tarjetas.forEach(tarjeta => {
      tarjeta.classList.remove('activo');
      tarjeta.classList.remove('oculto');
    });
  }

  function abrirDetalle(clave, tarjeta) {
    const servicio = dataServicios[clave];
    if (!servicio) return;

    equipoActivo = servicio.nombre;

    descripcionDetalle.textContent = servicio.descripcion;

    cargarMarcas(servicio.marcas);
    cargarCasos(servicio.casos);
    limpiarFormulario();

    // Ejemplos del formulario según el equipo seleccionado
    modeloEquipo.placeholder = servicio.ejemploModelo || 'Ejemplo: marca y modelo del equipo';
    fallaEquipo.placeholder = servicio.ejemploFalla || 'Describe brevemente la falla que presenta';

    panelDefault.classList.add('hidden');
    panelDetalle.classList.remove('hidden');

    activarSoloUnaTarjeta(tarjeta);

    const y = panelServicio.getBoundingClientRect().top + window.pageYOffset - 95;

window.scrollTo({
  top: y,
  behavior: 'smooth'
});
  }

  function volverAVistaGeneral() {
    equipoActivo = '';
    panelDetalle.classList.add('hidden');
    panelDefault.classList.remove('hidden');
    mostrarTodasLasTarjetas();
    limpiarFormulario();

    // Reposiciona la vista en las categorías para que queden centradas/visibles
    const seccion = document.getElementById('servicios');
    if (seccion) {
      const y = seccion.getBoundingClientRect().top + window.pageYOffset - 80;
      window.scrollTo({ top: y, behavior: 'smooth' });
    }
  }

  tarjetas.forEach(tarjeta => {
    tarjeta.addEventListener('click', () => {
      const clave = tarjeta.dataset.equipo;
      abrirDetalle(clave, tarjeta);
    });
  });

  btnVolver.addEventListener('click', volverAVistaGeneral);

  btnWhatsapp.addEventListener('click', () => {
    const marca = marcaEquipo.value || 'No especificada';
    const modelo = modeloEquipo.value.trim() || 'No especificado';
    const falla = fallaEquipo.value.trim() || 'No indicó la falla';

    const mensaje =
      `Hola, PanaRepara.%0A%0A` +
      `Servicio seleccionado: ${equipoActivo || 'No especificado'}%0A` +
      `Marca: ${marca}%0A` +
      `Modelo: ${modelo}%0A` +
      `Resumen de la falla: ${falla}%0A%0A` +
      `Quisiera más información y una cotización.`;

    window.open(`https://wa.me/${telefonoWhatsApp}?text=${mensaje}`, '_blank');
  });

  // -------------------- Carrusel de trabajos --------------------
  const carousel = document.getElementById('galeriaCarousel');

  if (carousel) {
    const totalFotos = parseInt(carousel.dataset.fotos, 10) || 0;
    const dotsWrap = carousel.querySelector('#carouselDots');
    const reducirMovimiento = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

    // Construir las fotos del carrusel
    const slides = [];
    for (let i = 1; i <= totalFotos; i++) {
      const slide = document.createElement('div');
      slide.className = 'slide' + (i === 1 ? ' active' : '');
      const img = document.createElement('img');
      img.src = 'images/galeria-' + String(i).padStart(2, '0') + '.jpg';
      img.alt = 'Trabajo de reparación realizado en PanaRepara';
      img.loading = i <= 2 ? 'eager' : 'lazy';
      img.decoding = 'async';
      slide.appendChild(img);
      carousel.insertBefore(slide, carousel.querySelector('.carousel-prev'));
      slides.push(slide);
    }

    let actual = 0;
    let timer = null;

    slides.forEach((_, i) => {
      const dot = document.createElement('button');
      dot.type = 'button';
      dot.setAttribute('aria-label', 'Ver foto ' + (i + 1));
      dot.addEventListener('click', () => { mostrar(i); reiniciar(); });
      dotsWrap.appendChild(dot);
    });
    const dots = [...dotsWrap.children];

    function mostrar(i) {
      actual = (i + slides.length) % slides.length;
      slides.forEach((s, idx) => s.classList.toggle('active', idx === actual));
      dots.forEach((d, idx) => d.classList.toggle('active', idx === actual));
    }

    function siguiente() { mostrar(actual + 1); }

    function iniciar() {
      if (reducirMovimiento) return;
      timer = setInterval(siguiente, 3500);
    }

    function reiniciar() { clearInterval(timer); iniciar(); }

    mostrar(0);
    iniciar();

    carousel.querySelector('.carousel-next').addEventListener('click', () => { siguiente(); reiniciar(); });
    carousel.querySelector('.carousel-prev').addEventListener('click', () => { mostrar(actual - 1); reiniciar(); });

    carousel.addEventListener('mouseenter', () => clearInterval(timer));
    carousel.addEventListener('mouseleave', iniciar);
  }
});
