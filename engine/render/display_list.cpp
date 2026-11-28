#include "display_list.h"

namespace Veltrix {
void DisplayList::addRect(float x,float y,float width,float height,Color color){
    rects.push_back({x,y,width,height,color});
}
void DisplayList::addText(float x,float y,float size,int weight,const std::string& text,Color color){
    textItems.push_back({x,y,size,weight,text,color});
}
void DisplayList::addElement(PaintElementType type,float x,float y,float width,float height,Color color,const std::string& source,const std::string& alt,const std::string& value,const std::string& typeAttribute){
    elementItems.push_back({type,x,y,width,height,color,source,alt,value,typeAttribute});
}
const std::vector<PaintRect>& DisplayList::rectangles() const{return rects;}
const std::vector<PaintText>& DisplayList::texts() const{return textItems;}
const std::vector<PaintElement>& DisplayList::elements() const{return elementItems;}
}