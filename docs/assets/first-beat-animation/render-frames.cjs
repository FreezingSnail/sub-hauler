'use strict';
const fs=require('node:fs'),zlib=require('node:zlib'),crypto=require('node:crypto');
const rootOutput='docs/assets/first-beat-animation/';
const output=rootOutput+'all-beats-v3/';
function decode(path){const png=fs.readFileSync(path),parts=[];if(png.readUInt32BE(16)!==128||png.readUInt32BE(20)!==64||png[24]!==8||png[25]!==0)throw Error('Expected128x64 grayscale source');for(let p=8;p<png.length;){const n=png.readUInt32BE(p);if(png.toString('ascii',p+4,p+8)==='IDAT')parts.push(png.subarray(p+8,p+8+n));p+=n+12;}const raw=zlib.inflateSync(Buffer.concat(parts)),pixels=new Uint8Array(8192);for(let y=0;y<64;y++){if(raw[y*129]!==0)throw Error('Expected unfiltered source');for(let x=0;x<128;x++)pixels[y*128+x]=raw[y*129+x+1];}return pixels;}
function chunk(type,data){const body=Buffer.concat([Buffer.from(type),data]),size=Buffer.alloc(4),tail=Buffer.alloc(4);size.writeUInt32BE(data.length);let c=0xffffffff;for(const v of body){c^=v;for(let b=0;b<8;b++)c=(c>>>1)^((c&1)?0xedb88320:0);}tail.writeUInt32BE((c^0xffffffff)>>>0);return Buffer.concat([size,body,tail]);}
function encode(pixels,w=128,h=64){const hdr=Buffer.alloc(13),raw=Buffer.alloc((w+1)*h);hdr.writeUInt32BE(w);hdr.writeUInt32BE(h,4);hdr[8]=8;for(let y=0;y<h;y++)for(let x=0;x<w;x++)raw[y*(w+1)+1+x]=pixels[y*w+x];return Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',hdr),chunk('IDAT',zlib.deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]);}
function inside(x,y,poly){let yes=false;for(let i=0,j=poly.length-1;i<poly.length;j=i++){const a=poly[i],b=poly[j];if((a[1]>y)!==(b[1]>y)&&x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0])yes=!yes;}return yes;}
const scenes=[
  {
    "key": "arrival",
    "source": "docs/assets/story-underwater-v7/arrival-native.png",
    "polygon": [
      [
        2,
        28
      ],
      [
        3,
        23
      ],
      [
        9,
        22
      ],
      [
        12,
        24
      ],
      [
        40,
        23
      ],
      [
        41,
        14
      ],
      [
        44,
        14
      ],
      [
        44,
        5
      ],
      [
        53,
        5
      ],
      [
        53,
        21
      ],
      [
        55,
        20
      ],
      [
        60,
        20
      ],
      [
        60,
        23
      ],
      [
        66,
        26
      ],
      [
        66,
        37
      ],
      [
        60,
        40
      ],
      [
        14,
        41
      ],
      [
        11,
        44
      ],
      [
        5,
        44
      ],
      [
        7,
        39
      ],
      [
        2,
        38
      ]
    ],
    "offsets": [
      [
        0,
        0
      ],
      [
        1,
        0
      ],
      [
        1,
        -1
      ],
      [
        0,
        -1
      ]
    ],
    "action": "Hull/periscope sprite drifts right1px and bobs up1px. Seabed habitat, water and all outside-mask dithering stay locked."
  },
  {
    "key": "grow-tanks",
    "source": "docs/assets/first-beat-clarity-v6/grow-tanks-native.png",
    "regions": [
      [
        57,
        30,
        5,
        12
      ],
      [
        78,
        30,
        5,
        12
      ]
    ],
    "action": "Two tiny bubble highlights rise through existing tank water. Roots, tank rims, plants and lamps stay fixed."
  },
  {
    "key": "repair-bay",
    "source": "docs/assets/first-beat-clarity-v6/repair-bay-native.png",
    "regions": [
      [
        104,
        22,
        5,
        4
      ],
      [
        75,
        9,
        10,
        6
      ]
    ],
    "action": "Existing broken-cable tip sways1px; a small inspection-lamp highlight pulses. Impeller and freight platform never move."
  },
  {
    "key": "pump-approach",
    "source": "docs/assets/pump-survey/approach-internal-v2-native.png",
    "regions": [
      [
        7,
        30,
        5,
        8
      ]
    ],
    "bubbles": [
      {
        "x": 8,
        "start": 36
      }
    ],
    "action": "Small rising underwater bubble highlights; hull, machinery, background and texture remain fixed."
  },
  {
    "key": "pump-cracked-impeller",
    "source": "docs/assets/pump-survey/cracked-impeller-native.png",
    "regions": [
      [
        115,
        16,
        5,
        8
      ]
    ],
    "bubbles": [
      {
        "x": 116,
        "start": 22
      }
    ],
    "action": "Small rising underwater bubble highlights; hull, machinery, background and texture remain fixed."
  },
  {
    "key": "delivery-loading",
    "source": "docs/assets/pump-delivery/loading-impeller-balanced-v4-native.png",
    "regions": [
      [
        101,
        12,
        9,
        7
      ]
    ],
    "glint": [
      101,
      12,
      9,
      7
    ],
    "action": "A small fixture highlight gently pulses; loading cargo or machinery remains fixed."
  },
  {
    "key": "delivery-water",
    "source": "docs/assets/pump-delivery/water-restored-native.png",
    "regions": [
      [
        84,
        44,
        5,
        8
      ]
    ],
    "bubbles": [
      {
        "x": 85,
        "start": 50
      }
    ],
    "action": "Small rising underwater bubble highlights; hull, machinery, background and texture remain fixed."
  },
  {
    "key": "lift-yard",
    "source": "docs/assets/lift-survey/freight-yard-balanced-v3-native.png",
    "regions": [
      [
        6,
        29,
        5,
        8
      ]
    ],
    "bubbles": [
      {
        "x": 7,
        "start": 35
      }
    ],
    "action": "Small rising underwater bubble highlights; hull, machinery, background and texture remain fixed."
  },
  {
    "key": "lift-cable",
    "source": "docs/assets/lift-survey/ruined-cable-clear-sub-v3-native.png",
    "regions": [
      [
        117,
        45,
        5,
        8
      ]
    ],
    "bubbles": [
      {
        "x": 118,
        "start": 51
      }
    ],
    "action": "Small rising underwater bubble highlights; hull, machinery, background and texture remain fixed."
  },
  {
    "key": "lift-delivery-cable",
    "source": "docs/assets/story-underwater-support-v7/loading-cable-native.png",
    "regions": [
      [
        91,
        5,
        8,
        6
      ]
    ],
    "glint": [
      91,
      5,
      8,
      6
    ],
    "action": "A small fixture highlight gently pulses; loading cargo or machinery remains fixed."
  },
  {
    "key": "lift-delivery-working",
    "source": "docs/assets/story-underwater-support-v7/lift-restored-native.png",
    "regions": [
      [
        81,
        6,
        9,
        7
      ]
    ],
    "glint": [
      81,
      6,
      9,
      7
    ],
    "action": "A small fixture highlight gently pulses; loading cargo or machinery remains fixed."
  },
  {
    "key": "beacon-descent",
    "source": "docs/assets/beacon-delivery/descent-native.png",
    "regions": [
      [
        18,
        7,
        5,
        8
      ]
    ],
    "bubbles": [
      {
        "x": 19,
        "start": 13
      }
    ],
    "action": "Small rising underwater bubble highlights; hull, machinery, background and texture remain fixed."
  },
  {
    "key": "beacon-lit",
    "source": "docs/assets/beacon-delivery/beacon-lit-native.png",
    "regions": [
      [
        98,
        8,
        12,
        12
      ]
    ],
    "glint": [
      98,
      8,
      12,
      12
    ],
    "action": "A small fixture highlight gently pulses; loading cargo or machinery remains fixed."
  },
  {
    "key": "home-return",
    "source": "docs/assets/story-underwater-v7/home-return-native.png",
    "regions": [
      [
        23,
        13,
        5,
        8
      ]
    ],
    "bubbles": [
      {
        "x": 24,
        "start": 19
      }
    ],
    "action": "Small rising underwater bubble highlights; hull, machinery, background and texture remain fixed."
  },
  {
    "key": "home-lights",
    "source": "docs/assets/story-underwater-v7/home-lights-native.png",
    "regions": [
      [
        6,
        14,
        5,
        8
      ],
      [
        92,
        12,
        8,
        7
      ]
    ],
    "bubbles": [
      {
        "x": 7,
        "start": 20
      }
    ],
    "action": "Small trailing bubbles and a signal-lamp highlight; moored hull, line and seabed habitat remain fixed.",
    "glint": [
      92,
      12,
      8,
      7
    ]
  }
];
function render(scene,base,phase){const out=Uint8Array.from(base),mask=new Uint8Array(8192);const mark=(x,y)=>{if(x>=0&&x<128&&y>=0&&y<64)mask[y*128+x]=1;};const put=(x,y,v)=>{if(x>=0&&x<128&&y>=0&&y<64){if(!mask[y*128+x])throw Error('Write outside explicit motion mask');out[y*128+x]=v;}};if(scene.polygon){const points=[];for(let y=0;y<64;y++)for(let x=0;x<128;x++)if(inside(x,y,scene.polygon)){points.push([x,y,base[y*128+x]]);for(const [dx,dy]of scene.offsets)mark(x+dx,y+dy);}if(phase){for(const[x,y]of points)put(x,y,0);const[dx,dy]=scene.offsets[phase];for(const[x,y,v]of points)put(x+dx,y+dy,v);}}
 else{for(const[x,y,w,h]of scene.regions)for(let yy=y;yy<y+h;yy++)for(let xx=x;xx<x+w;xx++)mark(xx,yy);if(phase&&scene.bubbles){for(const bubble of scene.bubbles){const y=bubble.start-2*(phase-1),x=bubble.x;put(x,y,255);put(x+1,y-1,255);put(x+2,y,255);put(x+1,y+1,255);}}if(phase&&scene.glint){const[x,y,w,h]=scene.glint,candidates=[];for(let yy=y;yy<y+h;yy++)for(let xx=x;xx<x+w;xx++)if(!base[yy*128+xx]){const adjacent=[[xx-1,yy],[xx+1,yy],[xx,yy-1],[xx,yy+1]].some(([u,v])=>u>=0&&u<128&&v>=0&&v<64&&base[v*128+u]);candidates.push({x:xx,y:yy,score:(adjacent?0:1000)+(xx-x-w/2)**2+(yy-y-h/2)**2});}candidates.sort((a,b)=>a.score-b.score||a.y-b.y||a.x-b.x);if(candidates.length<4)throw Error('Glint patch too bright '+scene.key);for(const point of candidates.slice(0,[0,3,7,2][phase]))put(point.x,point.y,255);}if(scene.key==='grow-tanks'&&phase){for(const[x,start]of [[58,39],[79,40]]){const y=start-2*(phase-1);put(x,y,255);put(x+1,y-1,255);put(x+2,y,255);put(x+1,y+1,255);}}
 if(scene.key==='repair-bay'&&phase){put(105,23,0);put(106,24,0);const dx=[0,1,0,-1][phase];put(105+dx,23,255);put(106+dx,24,255);for(const[x,y]of (phase===1?[[77,10],[78,10],[79,10]]:phase===2?[[76,10],[77,10],[78,10],[79,10],[80,10],[81,10],[82,9],[83,11]]:[[78,10]]))put(x,y,255);}}
 return{pixels:out,mask};}
function renderAll(){fs.mkdirSync(output,{recursive:true});const manifest={period:300,scenes:{}},metadata={periodMs:300,poses:4,startsPaused:true,scenes:[],rendererFile:rootOutput+'render-frames.cjs',rendererCode:fs.readFileSync(__filename,'utf8')};for(const scene of scenes){const base=decode(scene.source),frames=[],stats=[];for(let p=0;p<4;p++){const {pixels,mask}=render(scene,base,p);let changed=0;for(let i=0;i<8192;i++){if(pixels[i]!==0&&pixels[i]!==255)throw Error('Nonbinary pixel');if(pixels[i]!==base[i]){changed++;if(!mask[i])throw Error('Change outside mask');}}if(p===0&&changed)throw Error('Phase0 drift');if(p>0&&!changed)throw Error('Empty motion pose');const file=scene.key+'-'+p+'.png';fs.writeFileSync(output+file,encode(pixels));frames.push(pixels);stats.push(changed);}if(new Set(frames.map(p=>Buffer.from(p).toString('base64'))).size!==4)throw Error('Duplicate poses '+scene.key);manifest.scenes[scene.key]=[0,1,2,3].map(p=>'assets/first-beat-animation/all-beats-v3/'+scene.key+'-'+p+'.png');const sheet=new Uint8Array(2048*256);for(let y=0;y<256;y++)for(let x=0;x<2048;x++)sheet[y*2048+x]=frames[Math.floor(x/512)][Math.floor(y/4)*128+Math.floor(x%512/4)];fs.writeFileSync(output+scene.key+'-contact-4x.png',encode(sheet,2048,256));metadata.scenes.push({...scene,sourceSha256:crypto.createHash('sha256').update(fs.readFileSync(scene.source)).digest('hex'),changedPixels:stats,contactSheet:output+scene.key+'-contact-4x.png'});console.log(scene.key+': changed pixels '+stats.join('/')+'; phase0 exact; outside masks unchanged;128x64 opaque BW');}const comparison=new Uint8Array(1024*256),compareSources=[decode('docs/assets/first-beat-clarity-v6/arrival-native.png'),decode(scenes[0].source)];for(let y=0;y<256;y++)for(let x=0;x<1024;x++)comparison[y*1024+x]=compareSources[Math.floor(x/512)][Math.floor(y/4)*128+Math.floor(x%512/4)];fs.writeFileSync(output+'arrival-before-after-4x.png',encode(comparison,1024,256));metadata.scope='All15imagecards across7campaignbeats';fs.writeFileSync(rootOutput+'frames.js','globalThis.StoryAnimation='+JSON.stringify(manifest)+';globalThis.FirstBeatAnimation=globalThis.StoryAnimation;\n');fs.writeFileSync(rootOutput+'animation.json',JSON.stringify(metadata,null,2)+'\n');}
if(require.main===module)renderAll();module.exports={decode,encode,scenes,render,renderAll};
