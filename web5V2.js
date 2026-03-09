document.addEventListener('DOMContentLoaded', () => {
  const tarjetas = document.querySelectorAll('.equipo-card');

  const panelServicio = document.getElementById('panelServicio');
  const panelDefault = document.getElementById('panelDefault');
  const panelDetalle = document.getElementById('panelDetalle');

  const tituloDetalle = document.getElementById('tituloDetalle');
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
      titulo: 'Reparación de televisores en El Dorado, Ciudad de Panamá',
      descripcion: 'Diagnóstico y reparación de televisores LED, Smart TV y pantallas con fallas de imagen, retroiluminación, audio, fuente de poder y tarjeta principal. Atendemos en Centro Comercial El Dorado, segundo piso, local 52A.',
      marcas: ['Samsung', 'LG', 'Sony', 'Hisense', 'TCL', 'Philips', 'Panasonic', 'RCA'],
      casos: [
        { titulo: 'TV sin imagen', texto: 'Revisión de retroiluminación, tiras LED, driver y fuente de poder.' },
        { titulo: 'TV no enciende', texto: 'Diagnóstico de standby, fusibles, fuente primaria y componentes dañados.' },
        { titulo: 'TV con sonido pero sin video', texto: 'Pruebas en panel, T-CON, backlight y tarjeta principal.' }
      ]
    },

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

    laptops: {
      nombre: 'Laptops',
      titulo: 'Reparación de laptops en El Dorado, Ciudad de Panamá',
      descripcion: 'Diagnóstico y reparación de laptops con fallas de encendido, video, carga, teclado, batería, bisagras y sobrecalentamiento.',
      marcas: ['HP', 'Dell', 'Lenovo', 'Asus', 'Acer', 'Apple', 'MSI', 'Toshiba'],
      casos: [
        { titulo: 'Laptop no enciende', texto: 'Revisión de entrada DC, circuito de carga y etapas primarias.' },
        { titulo: 'Laptop sin video', texto: 'Diagnóstico de memoria, BIOS, pantalla, flex y video.' },
        { titulo: 'Sobrecalentamiento', texto: 'Mantenimiento preventivo, limpieza y cambio de pasta térmica.' }
      ]
    },

    impresoras: {
      nombre: 'Impresoras',
      titulo: 'Reparación de impresoras en El Dorado, Ciudad de Panamá',
      descripcion: 'Servicio técnico especializado en impresoras Epson, HP, Canon y Brother. Realizamos cambio de cabezales, limpieza profunda, solución de errores de tinta, mantenimiento y reseteo de almohadillas.',
      marcas: ['Epson', 'HP', 'Canon', 'Brother'],
      casos: [
        { titulo: 'Cambio de cabezal', texto: 'Reemplazo y prueba de cabezal en impresoras con líneas o falta de color.' },
        { titulo: 'Limpieza profunda', texto: 'Limpieza interna, estación de servicio, sistema de tinta y mantenimiento.' },
        { titulo: 'Error de almohadillas', texto: 'Diagnóstico, mantenimiento y reseteo en impresoras Epson.' }
      ]
    },

    bocinas: {
      nombre: 'Bocinas Portátiles',
      titulo: 'Reparación de bocinas portátiles en El Dorado, Ciudad de Panamá',
      descripcion: 'Diagnóstico y reparación de bocinas con fallas de carga, batería, Bluetooth, distorsión de audio y conectores dañados.',
      marcas: ['JBL', 'Bose', 'Sony', 'LG', 'Anker', 'Marshall'],
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
      const card = document.createElement('div');
      card.className = 'caso-card';
      card.innerHTML = `
        <h4>${caso.titulo}</h4>
        <p>${caso.texto}</p>
      `;
      casosReales.appendChild(card);
    });
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

    tituloDetalle.textContent = servicio.titulo;
    descripcionDetalle.textContent = servicio.descripcion;

    cargarMarcas(servicio.marcas);
    cargarCasos(servicio.casos);
    limpiarFormulario();

    panelDefault.classList.add('hidden');
    panelDetalle.classList.remove('hidden');

    activarSoloUnaTarjeta(tarjeta);

    panelServicio.scrollIntoView({
      behavior: 'smooth',
      block: 'start'
    });
  }

  function volverAVistaGeneral() {
    equipoActivo = '';
    panelDetalle.classList.add('hidden');
    panelDefault.classList.remove('hidden');
    mostrarTodasLasTarjetas();
    limpiarFormulario();
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
});
