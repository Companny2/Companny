document.addEventListener('DOMContentLoaded', () => {
  const items = document.querySelectorAll('.service-item .item');
  const serviceItems = document.querySelectorAll('.service-item');

  const panel = document.getElementById('panel-servicio');
  const titulo = document.getElementById('titulo-servicio');
  const desc = document.getElementById('descripcion-servicio');
  const cerrarBtn = document.querySelector('.cerrar-panel');

  const telefonoWhatsApp = '50760322222';
  const telefonoLlamada = '50760322222';

  const dataServicios = {
    'Televisores': {
      titulo: 'Reparación de televisores en El Dorado, Ciudad de Panamá',
      descripcion: 'Diagnóstico y reparación de televisores LED, Smart TV y pantallas con fallas de imagen, retroiluminación, audio, fuente de poder y tarjeta principal. Atendemos en Centro Comercial El Dorado, segundo piso, local 52A, Vía Ricardo J. Alfaro.',
      marcas: ['Samsung', 'LG', 'Sony', 'Hisense', 'TCL', 'Philips', 'Panasonic', 'RCA'],
      casos: [
        {
          titulo: 'TV sin imagen',
          texto: 'Revisión de retroiluminación, tiras LED, driver y fuente de poder.'
        },
        {
          titulo: 'TV no enciende',
          texto: 'Diagnóstico de standby, fusibles, fuente primaria y componentes dañados.'
        },
        {
          titulo: 'TV con sonido pero sin video',
          texto: 'Pruebas en panel, T-CON, backlight y tarjeta principal.'
        }
      ]
    },

    'Video Juegos': {
      titulo: 'Reparación de consolas en El Dorado, Ciudad de Panamá',
      descripcion: 'Servicio técnico para PlayStation, Xbox y Nintendo con fallas de HDMI, sobrecalentamiento, encendido, almacenamiento y mantenimiento interno.',
      marcas: ['PS5', 'PS4', 'Xbox Series', 'Xbox One', 'Nintendo Switch'],
      casos: [
        {
          titulo: 'Puerto HDMI dañado',
          texto: 'Cambio de HDMI en consolas sin señal o con imagen intermitente.'
        },
        {
          titulo: 'Consola se apaga',
          texto: 'Mantenimiento interno, limpieza y revisión térmica.'
        },
        {
          titulo: 'Sobrecalentamiento',
          texto: 'Cambio de pasta térmica y limpieza profunda.'
        }
      ]
    },

    'Laptops': {
      titulo: 'Reparación de laptops en El Dorado, Ciudad de Panamá',
      descripcion: 'Diagnóstico y reparación de laptops con fallas de encendido, video, carga, teclado, batería, bisagras y sobrecalentamiento.',
      marcas: ['HP', 'Dell', 'Lenovo', 'Asus', 'Acer', 'Apple', 'MSI', 'Toshiba'],
      casos: [
        {
          titulo: 'Laptop no enciende',
          texto: 'Revisión de entrada DC, circuito de carga y etapas primarias.'
        },
        {
          titulo: 'Laptop sin video',
          texto: 'Diagnóstico de memoria, BIOS, pantalla, flex y video.'
        },
        {
          titulo: 'Sobrecalentamiento',
          texto: 'Mantenimiento preventivo, limpieza y cambio de pasta térmica.'
        }
      ]
    },

    'Impresoras': {
      titulo: 'Reparación de impresoras en El Dorado, Ciudad de Panamá',
      descripcion: 'Servicio técnico especializado en impresoras Epson, HP, Canon y Brother. Realizamos cambio de cabezales, limpieza profunda, solución de errores de tinta, mantenimiento y reseteo de almohadillas.',
      marcas: ['Epson', 'HP', 'Canon', 'Brother'],
      casos: [
        {
          titulo: 'Cambio de cabezal',
          texto: 'Reemplazo y prueba de cabezal en impresoras con líneas o falta de color.'
        },
        {
          titulo: 'Limpieza profunda',
          texto: 'Limpieza interna, estación de servicio, sistema de tinta y mantenimiento.'
        },
        {
          titulo: 'Error de almohadillas',
          texto: 'Diagnóstico, mantenimiento y reseteo en impresoras Epson.'
        }
      ]
    },

    'Bocinas Portátiles': {
      titulo: 'Reparación de bocinas portátiles en El Dorado, Ciudad de Panamá',
      descripcion: 'Diagnóstico y reparación de bocinas con fallas de carga, batería, Bluetooth, distorsión de audio y conectores dañados.',
      marcas: ['JBL', 'Bose', 'Sony', 'LG', 'Anker', 'Marshall'],
      casos: [
        {
          titulo: 'Bocina no carga',
          texto: 'Revisión de batería, puerto de carga y circuito de alimentación.'
        },
        {
          titulo: 'Audio distorsionado',
          texto: 'Diagnóstico de parlantes, amplificación y sistema interno.'
        },
        {
          titulo: 'Bluetooth inestable',
          texto: 'Revisión del módulo Bluetooth y alimentación.'
        }
      ]
    }
  };

  function crearPanelDinamicoSiNoExiste() {
    if (!document.getElementById('extra-servicio-dinamico')) {
      const extra = document.createElement('div');
      extra.id = 'extra-servicio-dinamico';
      extra.innerHTML = `
        <div class="casos-wrap">
          <h3>Casos y servicios frecuentes</h3>
          <div id="casos-servicio" class="casos-servicio"></div>
        </div>

        <div class="form-servicio-dinamico">
          <h3>Cuéntanos tu caso</h3>

          <label for="marca-equipo">Marca</label>
          <select id="marca-equipo">
            <option value="">Selecciona una marca</option>
          </select>

          <label for="modelo-equipo">Modelo</label>
          <input type="text" id="modelo-equipo" placeholder="Ejemplo: Epson L3250">

          <label for="falla-equipo">Resumen de la falla</label>
          <textarea id="falla-equipo" rows="5" placeholder="Describe la falla de tu equipo"></textarea>

          <div class="acciones-servicio-dinamico">
            <button type="button" id="btn-whatsapp-servicio">Enviar por WhatsApp</button>
            <a id="btn-llamar-servicio" href="tel:+50760322222">Llamar ahora</a>
            <button type="button" id="btn-volver-servicio">Volver a todas las categorías</button>
          </div>
        </div>
      `;
      panel.appendChild(extra);
    }
  }

  function cargarMarcas(marcas) {
    const selectMarca = document.getElementById('marca-equipo');
    if (!selectMarca) return;

    selectMarca.innerHTML = '<option value="">Selecciona una marca</option>';

    marcas.forEach((marca) => {
      const option = document.createElement('option');
      option.value = marca;
      option.textContent = marca;
      selectMarca.appendChild(option);
    });
  }

  function cargarCasos(casos) {
    const contenedor = document.getElementById('casos-servicio');
    if (!contenedor) return;

    contenedor.innerHTML = '';

    casos.forEach((caso) => {
      const card = document.createElement('div');
      card.className = 'caso-card';
      card.innerHTML = `
        <h4>${caso.titulo}</h4>
        <p>${caso.texto}</p>
      `;
      contenedor.appendChild(card);
    });
  }

  function ocultarOtrasCategorias(nombreSeleccionado) {
    serviceItems.forEach((serviceItem) => {
      const label = serviceItem.querySelector('.service-label');
      if (!label) return;

      const nombre = label.innerText.trim();

      if (nombre !== nombreSeleccionado) {
        serviceItem.style.display = 'none';
      } else {
        serviceItem.style.display = 'block';
      }
    });
  }

  function mostrarTodasLasCategorias() {
    serviceItems.forEach((serviceItem) => {
      serviceItem.style.display = 'block';
    });
  }

  function configurarBotones(nombreServicio) {
    const btnWhatsapp = document.getElementById('btn-whatsapp-servicio');
    const btnLlamar = document.getElementById('btn-llamar-servicio');
    const btnVolver = document.getElementById('btn-volver-servicio');

    if (btnLlamar) {
      btnLlamar.setAttribute('href', `tel:+${telefonoLlamada}`);
    }

    if (btnWhatsapp) {
      btnWhatsapp.onclick = () => {
        const marca = document.getElementById('marca-equipo')?.value || 'No especificada';
        const modelo = document.getElementById('modelo-equipo')?.value.trim() || 'No especificado';
        const falla = document.getElementById('falla-equipo')?.value.trim() || 'No indicó la falla';

        const mensaje =
          `Hola, PanaRepara.%0A%0A` +
          `Servicio seleccionado: ${nombreServicio}%0A` +
          `Marca: ${marca}%0A` +
          `Modelo: ${modelo}%0A` +
          `Resumen de la falla: ${falla}%0A%0A` +
          `Quisiera más información y una cotización.`;

        window.open(`https://wa.me/${telefonoWhatsApp}?text=${mensaje}`, '_blank');
      };
    }

    if (btnVolver) {
      btnVolver.onclick = () => {
        mostrarTodasLasCategorias();
        panel.classList.remove('mostrar');
      };
    }
  }

  items.forEach((item) => {
    item.addEventListener('click', () => {
      const label = item.parentElement.querySelector('.service-label');
      if (!label) return;

      const name = label.innerText.trim();
      const data = dataServicios[name];

      if (!data) return;

      crearPanelDinamicoSiNoExiste();

      titulo.textContent = data.titulo;
      desc.textContent = data.descripcion;

      cargarMarcas(data.marcas);
      cargarCasos(data.casos);
      configurarBotones(name);
      ocultarOtrasCategorias(name);

      panel.classList.add('mostrar');
      panel.scrollIntoView({
        behavior: 'smooth',
        block: 'start'
      });
    });
  });

  if (cerrarBtn) {
    cerrarBtn.addEventListener('click', () => {
      panel.classList.remove('mostrar');
      mostrarTodasLasCategorias();
    });
  }
});
