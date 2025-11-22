
document.querySelectorAll('.service-item .item').forEach(item=>{
  item.addEventListener('click',()=>{
    const panel=document.getElementById('panel-servicio');
    const titulo=document.getElementById('titulo-servicio');
    const desc=document.getElementById('descripcion-servicio');

    const name=item.parentElement.querySelector('.service-label').innerText;

    const textos={
      'Televisores':'LCD, LED, OLED, reparación de tarjetas y más.',
      'Video Juegos':'PlayStation, Nintendo, Xbox y más.',
      'Laptops':'Mantenimiento, teclado, batería, software.',
      'Impresoras':'Cabezales, limpieza, tinta, mantenimiento.',
      'Bocinas Portátiles':'Sonido, batería y conectores.'
    };

    titulo.textContent=name;
    desc.textContent=textos[name];

    panel.classList.add('mostrar');
  });
});

document.querySelector('.cerrar-panel').addEventListener('click',()=>{
  document.getElementById('panel-servicio').classList.remove('mostrar');
});
