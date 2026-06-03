
function random10to50() {
  return Math.floor(Math.random() * 41) + 10;
}

function load(){
 fetch('http://192.168.0.170/status'), {
  method: 'POST',
  mode: 'no-cors'}
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
   StK.value=d.StK+"°C"; 
   StWG.value=d.StWG+"°C";
   StWD.value=d.StWD+"°C"; 
   BtG.value=d.BtG+"°C"; 
   BtD.value=d.BtD+"°C"; 
   tZ.value=d.tZ+"°C"; 
   tD.value=d.tD+"°C"; 
   tM.value=d.tM+"°C";
   Sp.value=d.Sp+"°C"; 
   Wp.value=d.Wp?"ON":"OFF"; 
   Op.value=d.Op?"ON":"OFF"; 
   don.value=d.don;
   doff.value=d.doff;
   max.value=d.max;
   mode.value=d.mode;
 });

   StK.value=random10to50()+"°C"; 
   StWG.value=random10to50()+"°C"; 
   StWD.value=random10to50()+"°C"; 
   don.value=random10to50();
   doff.value=random10to50();
   max.value=85;
   mode.value=1;
}

function save(){
 fetch(`http://192.168.0.170/save?don=${don.value}&doff=${doff.value}&max=${max.value}&mode=${mode.value}`);
}

setInterval(load,5000);
load();


function showPage(page, el) {

  // aktywny stan
  document.querySelectorAll('.nav-item').forEach(i => i.classList.remove('active'));
  el.classList.add('active');

  title.innerHTML=page
  if (page === "Ogrzewanie") {
    Ogrzewanie.style.display = "block"; 
  }else{
    Ogrzewanie.style.display = "none"; 
  }

  if (page === "Solary") {
    Solary.style.display = "block"; 
  }else{
    Solary.style.display = "none"; 
  }

  if (page === "Wykres") {
    Wykres.style.display = "block"; 
  }else{
    Wykres.style.display = "none"; 
  }

}

// start
showPage('Ogrzewanie', document.querySelector('.nav-item'));

/*
function sendToGoogleForm() {
  const url = "https://docs.google.com/forms/d/e/1FAIpQLSeMyHb_K9o5BwSu5TI9O8MQ973W9DqwT4RfNv4NN-t1LpUDQg/formResponse";

  const data = new FormData();
  data.append("entry.1561554265", 61);
  data.append("entry.2118651019", 41);
  data.append("entry.607098449", 41);
  fetch(url, {
    method: "POST",
    mode: "no-cors",
    body: data
  });

  alert("Wysłano!");
}



const CSV_URL ="https://docs.google.com/spreadsheets/d/e/2PACX-1vTIo-0UREaUUsQabhvwHKmc9aE2vw-BZrLc5sER3FumTxucXr35FQ4Q-y-fnu6b8gBbCz2ieFgFKaHe/pub?output=csv";

let chart;

async function getData() {
  const res = await fetch(CSV_URL);
  const text = await res.text();

  const rows = text.trim().split("\n").slice(1);

  let labels = [];
  let t1 = [];
  let t2 = [];
  let t3 = [];

  rows.forEach(r => {
    const c = r.split(",");

    if (c.length >= 4) {
      labels.push(c[0]);     // data
      t1.push(Number(c[1])); // temp1
      t2.push(Number(c[2])); // temp2
      t3.push(Number(c[3])); // temp3
    }
  });

  return { labels, t1, t2, t3 };
}

async function draw() {
  const d = await getData();

  if (!chart) {
    chart = new Chart(document.getElementById("chart"), {
      type: "line",
      data: {
        labels: d.labels,
        datasets: [
          {
            label: "Temp 1",
            data: d.t1,
            borderWidth: 2
          },
          {
            label: "Temp 2",
            data: d.t2,
            borderWidth: 2
          },
          {
            label: "Temp 3",
            data: d.t3,
            borderWidth: 2
          }
        ]
      }
    });

  } else {
    chart.data.labels = d.labels;
    chart.data.datasets[0].data = d.t1;
    chart.data.datasets[1].data = d.t2;
    chart.data.datasets[2].data = d.t3;
    chart.update();
  }
}

// start
draw();

// 🔄 AUTO UPDATE (co 5 sekund)
setInterval(draw, 500000);
*/