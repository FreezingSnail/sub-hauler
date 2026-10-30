(function(root,factory){if(typeof module==='object'&&module.exports)module.exports=factory();else root.StoryPlayback=factory();})(globalThis,function(){
 'use strict';
 function createPlayback(options){
  let key=null,count=0,phase=0,clock=null;
  const schedule=options.schedule||setInterval,cancel=options.cancel||clearInterval;
  const state=()=>({key,count,phase,playing:clock!==null});
  const notify=()=>{if(options.onState)options.onState(state());};
  const draw=()=>{if(options.onFrame)options.onFrame(state());notify();};
  function pause(){if(clock!==null){cancel(clock);clock=null;}notify();}
  return {
   state,
   select(nextKey,nextCount=0){pause();key=nextKey;count=nextCount;phase=0;draw();},
   play(){if(!key||count<2||clock!==null)return;clock=schedule(()=>{phase=(phase+1)%count;draw();},options.period||300);notify();},
   pause,
   step(){pause();if(!key||!count)return;phase=(phase+1)%count;draw();},
   restart(){pause();phase=0;draw();},
   destroy:pause
  };
 }
 return{createPlayback};
});
