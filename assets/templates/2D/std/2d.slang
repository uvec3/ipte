
namespace b2d
{
buffer Params_2d
{
	//preview parameters:
    bool showUVRect;
    bool showSpaceMesh;
    float previewScale;
};

float4 draw()
	if(showUVRect)
	{
	    float w=0.005/previewScale;
	    float2 fr= frac(uv);
	    if(step(-w,fr.y)-step(w,fr.y)+step(-w,fr.x)-step(w,fr.x))
	    	return 1-result;
	}

	if(showSpaceMesh)
	{
	    float w=0.005/previewScale;
	    float2 fr= frac(uvToSpacePoint(uv));
	    if(step(-w,fr.y)-step(w,fr.y)+step(-w,fr.x)-step(w,fr.x))
	    	return float4(0.5,0.5,1,1);
	}
}

void update()
{
    //scale with mouse wheel
    if(!isKeyDown(KeyCode::LeftShift)&&!isKeyDown(KeyCode::LeftAlt))
    {
        float2 mouseBeforeZoom=screenToUV(p.mouse_pos/p.extent*2-1);
        float zoomScale=exp(p.mouse_wheel_delta*0.1);
        float2 mouseAfterZoom=mouseBeforeZoom*zoomScale;
        p.zoom*=zoomScale;
        p.move-=(mouseBeforeZoom-mouseAfterZoom)/p.zoom;
     }
     else if(isKeyDown(KeyCode::LeftAlt))//rotate with Alt and mouse wheel
     {
         float delta=p.mouse_wheel_delta*0.1*radians(30);
         p.rotation+=delta;
         float2x2 rot=float2x2(cos(delta),sin(delta),-sin(delta),cos(delta));
         p.move=p.move*rot;
     }

    //movement with mouse
    if(p.lmb_down)
        p.move-=float2(p.mouse_delta.x,-p.mouse_delta.y)/p.extent.y/p.zoom*2/previewScale;

    //scale and move with keyboard
    float scaleUpdate=exp((isKeyDown(KeyCode::E)-isKeyDown(KeyCode::Q) )*p.dt);
    p.zoom=p.zoom*scaleUpdate;
    p.move-=(0.5-0.5*scaleUpdate)/p.zoom;

    float2 move_delta=float2(isKeyDown(KeyCode::D)-isKeyDown(KeyCode::A),isKeyDown(KeyCode::W)-isKeyDown(KeyCode::S));
    p.move+=move_delta*p.dt/p.zoom;

    //preview parameters control
    if(isKeyPressed(KeyCode::_1))
        showUVRect=!showUVRect;
    if(isKeyPressed(KeyCode::_2))
        showSpaceMesh=!showSpaceMesh;
    if(isKeyDown(KeyCode::LeftShift))
        previewScale*=exp(p.mouse_wheel_delta*0.1);

    //limit parameters
    if(p.zoom<=0)
        p.zoom=0.1;
    }
}