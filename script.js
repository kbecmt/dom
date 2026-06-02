
function load(){
 fetch('/status')
 .then(r=>r.json())
 .then(d=>{
   document.getElementById('data').innerHTML =
   "Kolektor: "+d.tC+" °C<br>"+
   "CWU dół: "+d.tL+" °C<br>"+
   "CWU góra: "+d.tH+" °C<br>"+
   "Bufor dół : "+d.tbL+" °C<br>"+
   "Bufor góra : "+d.tbH+" °C<br>"+
   "Pompa Solary: "+(d.p?"ON":"OFF")+"<br>"+
   "Pompa Woda: "+(d.wp?"ON":"OFF")+"<br>"

   don.value=d.don;
   doff.value=d.doff;
   max.value=d.max;
   mode.value=d.mode;
 });
}

function save(){
 fetch(`/save?don=${don.value}&doff=${doff.value}&max=${max.value}&mode=${mode.value}`);
}

setInterval(load,3000);
load();