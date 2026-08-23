#pragma once


#ifndef FRAMEWORK
    #define FRAMEWORK 2D_FRAMEWORK
#else
    "Another framework have been defined already!"
#endif

#include <std/input.slang>
#include <std/basic.slang>


namespace std::framework_2d
{
    buffer Params_2d
    {
        //preview parameters:
        bool showUVRect;
        bool showSpaceMesh;
        float previewScale;

        //space transform:
        float2 move;
        float zoom;
        float rotation;
    };


    float2 uvToSpacePoint(float2 uv)
    {
        float2x2 rot=float2x2(cos(rotation),sin(rotation),-sin(rotation),cos(rotation));
        return rot*(uv / zoom +move);
    }

    float2 spacePointToUv(float2 spacePoint)
    {
        float2x2 rot=float2x2(cos(rotation),sin(rotation),-sin(rotation),cos(rotation));
        return  (transpose(rot)*spacePoint-move)*zoom;
    }


    float2 screenToUV(float2 screenPosition)
    {
    	screenPosition.y=-screenPosition.y;
    	float2 identitySquare;
    	if(std::extent.x>std::extent.y)
    		identitySquare=float2(std::extent.y/std::extent.x,1);
        else
            identitySquare=float2(1,std::extent.x/std::extent.y);
    	return (screenPosition+identitySquare*previewScale*0.5)/identitySquare/previewScale;
    }

    //Function is called for each pixel
    //and returns color for this pixel
    //you can use it to preview output function
    //this does not affect the export result
    float4 main(float2 screen_pixel)
    {
        float2 uv = screenToUV(screen_pixel);

        float4 result = output(uv);


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

        return result;
    }

    void update()
    {
        //scale with mouse wheel
        if(!std::isKeyDown(std::KeyCode::LeftShift)&&!std::isKeyDown(std::KeyCode::LeftAlt))
        {
            float2 mouseBeforeZoom=screenToUV(std::mouse_pos/std::extent*2-1);
            float zoomScale=exp(std::mouse_wheel_delta*0.1);
            float2 mouseAfterZoom=mouseBeforeZoom*zoomScale;
            zoom*=zoomScale;
            move-=(mouseBeforeZoom-mouseAfterZoom)/zoom;
         }
         else if(std::isKeyDown(std::KeyCode::LeftAlt))//rotate with Alt and mouse wheel
         {
             float delta=std::mouse_wheel_delta*0.1*radians(30);
             rotation+=delta;
             float2x2 rot=float2x2(cos(delta),sin(delta),-sin(delta),cos(delta));
             move=move*rot;
         }

        //movement with mouse
        if(std::lmb_down)
            move-=float2(std::mouse_delta.x,-std::mouse_delta.y)/std::extent.y/zoom*2/previewScale;

        //scale and move with keyboard
        float scaleUpdate=exp((std::isKeyDown(std::KeyCode::E)-std::isKeyDown(std::KeyCode::Q) )*std::dt);
        zoom=zoom*scaleUpdate;
        move-=(0.5-0.5*scaleUpdate)/zoom;

        float2 move_delta=float2(std::isKeyDown(std::KeyCode::D)-std::isKeyDown(std::KeyCode::A),std::isKeyDown(std::KeyCode::W)-std::isKeyDown(std::KeyCode::S));
        move+=move_delta*std::dt/zoom;

        //preview parameters control
        if(std::isKeyPressed(std::KeyCode::_1))
            showUVRect=!showUVRect;
        if(std::isKeyPressed(std::KeyCode::_2))
            showSpaceMesh=!showSpaceMesh;
        if(std::isKeyDown(std::KeyCode::LeftShift))
            previewScale*=exp(std::mouse_wheel_delta*0.1);

        //limit parameters
        if(zoom<=0)
            zoom=0.1;
    }
}

float4 main(float2 screen_pixel)
{
    return std::framework_2d::main(screen_pixel);
}

void UpdateParameters()
{
	std::framework_2d::update();
	update(); 
}