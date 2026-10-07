const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const html = fs.readFileSync(path.join(__dirname, '../src/network/html/SyncPage.html'), 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
let checks = 0;
function check(value) { ++checks; assert.ok(value); }
const bookId = '0000000000000001';
const snapshot = {format:'yomuka-book-snapshot',formatVersion:1,bookId,exportedAt:0,units:{readerSettings:{updatedAt:0,data:{}}}};
async function page(search='?book=%2F%E6%9C%AC.epub', initStatus=200) {
  const elements = {}, calls = [], downloads = [], boxes = [1,2,4,8].map(value=>({value:String(value),checked:true}));
  function element(id) { return elements[id] ||= {disabled:false,textContent:'',events:{},addEventListener(type,fn){this.events[type]=fn;}}; }
  let exportStatus=200, receiveStatus=200, confirmValue=true, networkFailure=false;
  const context = {
    document:{getElementById:element,querySelectorAll:query=>query.includes(':checked')?boxes.filter(b=>b.checked):boxes,
      createElement:()=>({click(){downloads.push(this.download);},remove(){}}),body:{appendChild(){}}},
    location:{search},URLSearchParams,AbortController,Response,Blob,
    URL:{createObjectURL:()=> 'blob:mock',revokeObjectURL(){}},
    setTimeout:()=>1,clearTimeout(){},confirm:()=>confirmValue,
    FormData:class {append(name,file){this.file=file;this.name=name;}},
    fetch:async(url,options)=>{
      calls.push({url,options});
      if(networkFailure) throw new Error('connection lost');
      if(url.startsWith('/api/sync/book')) return new Response(initStatus===200?JSON.stringify({title:'本 <safe>',author:'著者',bookId}):'本を開いてください',{status:initStatus});
      if(url.startsWith('/api/sync/export')) return new Response(exportStatus===200?JSON.stringify(snapshot):'保存データを確認できません',{status:exportStatus});
      return new Response(receiveStatus===200?'受信しました。端末で確認してください。':'別の本です',{status:receiveStatus});
    }
  };
  await vm.runInNewContext(script,context,{filename:'SyncPage-inline.js'});
  return {elements,calls,downloads,boxes,setExportStatus:v=>exportStatus=v,setReceiveStatus:v=>receiveStatus=v,
    setConfirm:v=>confirmValue=v,setNetworkFailure:v=>networkFailure=v,
    choose:async file=>elements.file.events.change({target:{files:file?[file]:[]}}),
    click:async id=>elements[id].events.click()};
}
function file(data=snapshot,size) { const text=JSON.stringify(data);return {name:'shared.json',size:size??Buffer.byteLength(text),text:async()=>text}; }
(async()=>{
  const p=await page();
  check(p.elements.book.textContent==='本 <safe> / 著者');
  check(!p.elements.export.disabled && p.elements.receive.disabled);
  await p.click('export');
  check(p.calls.at(-1).url.endsWith('&units=15'));
  check(p.calls.at(-1).url.includes('%2F%E6%9C%AC.epub'));
  check(p.downloads[0]===bookId+'.json');
  check(!p.elements.export.disabled);
  p.boxes.forEach(b=>b.checked=false);const old=p.calls.length;await p.click('export');
  check(p.calls.length===old && p.elements.status.textContent.includes('1種類以上'));
  p.boxes[2].checked=true;await p.click('export');check(p.calls.at(-1).url.endsWith('&units=4'));
  p.setExportStatus(409);await p.click('export');check(p.elements.status.textContent.includes('保存データ'));check(!p.elements.export.disabled);
  await p.choose(file());check(!p.elements.receive.disabled);check(p.elements['file-info'].textContent.includes('shared.json'));
  p.setConfirm(false);const before=p.calls.length;await p.click('receive');check(p.calls.length===before);
  p.setConfirm(true);await p.click('receive');check(p.calls.at(-1).url.startsWith('/api/sync/upload'));check(p.elements.status.textContent.includes('端末で確認'));check(!p.elements.receive.disabled);
  p.setNetworkFailure(true);await p.click('receive');check(p.elements.status.textContent.includes('送信結果を確認できません'));check(!p.elements.receive.disabled);
  p.setNetworkFailure(false);p.setReceiveStatus(409);await p.click('receive');check(p.elements.status.textContent.includes('別の本'));
  await p.choose(file({...snapshot,bookId:'0000000000000002'}));check(p.elements.receive.disabled);check(p.elements.status.textContent.includes('別の本'));
  await p.choose(file(snapshot,65537));check(p.elements.receive.disabled);check(p.elements.status.textContent.includes('65,536'));
  await p.choose(file(snapshot,0));check(p.elements.receive.disabled);
  await p.choose({name:'bad.json',size:3,text:async()=>'{'});check(p.elements.receive.disabled);
  const missing=await page('');check(missing.elements.export.disabled && missing.calls.length===0);
  const invalid=await page('?book=%2Fbook.epub',409);check(invalid.elements.export.disabled && invalid.elements.file.disabled);check(invalid.elements.status.textContent.includes('本を開いて'));
  console.log(`PASS Web UI: ${checks} checks`);
})().catch(error=>{console.error(error);process.exitCode=1;});
