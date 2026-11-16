/* Read-only exact-asset previews. No placement solving or inferred layers. */
'use strict';
function tileLayerPixels(layer, data) {
    const pixels=new Uint8ClampedArray(64*4),height=data.height;
    const min=height?Math.min(...height):0,max=height?Math.max(...height):0;
    for(let i=0;i<64;i++){
        let rgb;
        if(layer==='height'&&height){const v=max===min?128:Math.round(255*(height[i]-min)/(max-min));rgb=[v,v,v];}
        if(layer==='normal'&&data.normal_xyz)rgb=data.normal_xyz.slice(i*3,i*3+3);
        if(layer==='occlusion'&&data.occlusion)rgb=Array(3).fill(data.occlusion[i]);
        if(layer==='emission'){
            // Intensity/artwork-colored emission has no live CGRAM here: show
            // its intensity in gray instead of inventing a palette color.
            if(data.emission_intensity)rgb=Array(3).fill(data.emission_intensity[i]);
            else if(data.emission_rgba){
                const rgba=data.emission_rgba.slice(i*4,i*4+4),power=rgba[3]/255;
                rgb=data.emission_color_mask?.[i]===0?Array(3).fill(rgba[3]):rgba.slice(0,3).map(v=>Math.round(v*power));
            }
        }
        if(!rgb){const v=((i%8+Math.floor(i/8))%2)?45:65;rgb=[v,v,v];}
        pixels.set([...rgb,255],i*4);
    }
    return pixels;
}
function renderTileLayerPreviews(parent, tile, layers, drawArt) {
    const grid=document.createElement('div');grid.className='tile-layer-previews';
    const data=layers?.fields||{};
    for(const [layer,label,field] of [['art','Art',null],['height','Height','height'],['normal','Normal','normal_xyz'],['emission','Emission',null],['occlusion','Occlusion','occlusion']]){
        const figure=document.createElement('figure'),caption=document.createElement('figcaption'),canvas=document.createElement('canvas'),note=document.createElement('div');
        caption.textContent=label;note.className='hint';canvas.width=canvas.height=8;
        if(layer==='art'){drawArt(canvas,tile.indices);note.textContent='Diagnostic palette';}
        else{
            const present=field?!!data[field]:!!(data.emission_intensity||data.emission_rgba);
            const ctx=canvas.getContext('2d'),image=ctx.createImageData(8,8);image.data.set(tileLayerPixels(layer,data));ctx.putImageData(image,0,0);
            note.textContent=!present?'Not authored':layer==='height'?`${Math.min(...data.height)} to ${Math.max(...data.height)}`:
                layer==='emission'&&(data.emission_intensity||data.emission_color_mask?.includes(0))?'Intensity / explicit color':'Authored';
        }
        canvas.title=`${label}: ${note.textContent}. Shared exact-asset layer, not placement offsets.`;
        figure.append(caption,canvas,note);grid.append(figure);
    }
    parent.append(grid);
    const hint=document.createElement('div');hint.className='hint';hint.textContent='Shared tile layers · excludes placement offsets and calculated heights. Height grayscale uses this tile’s min/max.';parent.append(hint);
}
if(typeof module!=='undefined')module.exports={tileLayerPixels};
if(typeof document!=='undefined'&&typeof renderDetail==='function'){
    const originalRenderDetail=renderDetail;
    renderDetail=function(){
        originalRenderDetail();
        const art=detail.querySelector('canvas.big');
        if(!art)return;
        const holder=document.createElement('div');
        renderTileLayerPreviews(holder,byId.get(selected),detailData?.layers,draw);
        art.replaceWith(holder);
    };
}
