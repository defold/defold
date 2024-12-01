#include <assert.h>
#include <algorithm>

#include "TracyPrint.hpp"
#include "TracyImGui.hpp"

extern ImTextureID zigzagTex;

namespace tracy
{

bool s_wasActive = false;

bool WasActive()
{
    if( s_wasActive )
    {
        s_wasActive = false;
        return true;
    }
    return false;
}


void DrawZigZag( ImDrawList* draw, const ImVec2& wpos, double start, double end, double h, uint32_t color )
{
    const auto v = ( end - start ) / ( h * 2 );
    draw->AddImage( zigzagTex, wpos + ImVec2( start, -h ), wpos + ImVec2( end, h ), ImVec2( 0, 0 ), ImVec2( v, 1 ), color );
}

void DrawStripedRect( ImDrawList* draw, const ImVec2& wpos, double x0, double y0, double x1, double y1, double sw, uint32_t color, bool fix_stripes_in_screen_space, bool inverted )
{
    assert( x1 >= x0 );
    assert( y1 >= y0 );
    assert( sw > 0 );

    const auto ww = ImGui::GetItemRectSize().x;
    if( x0 > ww || x1 < 0 ) return;

    if( x1 - x0 > ww )
    {
        x0 = std::max<double>( 0, x0 );
        x1 = std::min<double>( ww, x1 );
    }

    x0 += wpos.x;
    x1 += wpos.x;

    ImGui::PushClipRect( ImVec2( x0, y0 ), ImVec2( x1, y1 ), true );

    const auto rw = x1 - x0;
    const auto rh = y1 - y0;
    const auto cnt = int( ( rh + rw + sw*2 ) / ( sw*2 ) );
    auto v0 = ImVec2( x0, y0 - rw );

    if ( fix_stripes_in_screen_space )
    {
        const auto window_height = double( ImGui::GetWindowHeight() );
        const auto flipped_v0y = window_height - v0.y; //we transform into a y-is-up coordinate space to achieve upper-left to lower-right stripes. If we didn't, we would calculate values for lower-left to upper-right

        const auto manhattan_distance = x0 + flipped_v0y;
        const auto in_multiples_of_2_times_sw = int( manhattan_distance / ( sw*2 ) );

        const auto floored_manhatten_distance = double( in_multiples_of_2_times_sw*sw*2 ); //floor in terms of 2 * stripe width

        const auto corrected_flipped_v0y = ( floored_manhatten_distance - x0 ); //the corrected (floored) y respects the position of the stripes
        v0.y = window_height - corrected_flipped_v0y - double( inverted*sw ); //transform back into y-is-down imgui space
    }

    for( int i=0; i<cnt; i++ )
    {
        draw->PathLineTo( v0 + ImVec2( 0, i*sw*2 ) );
        draw->PathLineTo( v0 + ImVec2( rw, i*sw*2 + rw ) );
        draw->PathLineTo( v0 + ImVec2( rw, i*sw*2 + rw + sw ) );
        draw->PathLineTo( v0 + ImVec2( 0, i*sw*2 + sw ) );
        draw->PathFillConvex( color );
    }

    ImGui::PopClipRect();
}

void DrawHistogramMinMaxLabel( ImDrawList* draw, int64_t tmin, int64_t tmax, ImVec2 wpos, float w, float ty )
{
    const auto dpos = wpos + ImVec2( 0.5f, 0.5f );
    const auto ty15 = round( ty * 1.5f );
    const auto mintxt = TimeToString( tmin );
    const auto maxtxt = TimeToString( tmax );
    const auto maxsz = ImGui::CalcTextSize( maxtxt ).x;
    DrawLine( draw, dpos, dpos + ImVec2( 0, ty15 ), 0x66FFFFFF );
    DrawLine( draw, dpos + ImVec2( w-1, 0 ), dpos + ImVec2( w-1, ty15 ), 0x66FFFFFF );
    draw->AddText( wpos + ImVec2( 0, ty15 ), 0x66FFFFFF, mintxt );
    draw->AddText( wpos + ImVec2( w-1-maxsz, ty15 ), 0x66FFFFFF, maxtxt );

    char range[64];
    sprintf( range, ICON_FA_LEFT_LONG " %s " ICON_FA_RIGHT_LONG, TimeToString( tmax - tmin ) );

    const auto rsz = ImGui::CalcTextSize( range ).x;
    draw->AddText( wpos + ImVec2( round( (w-1-rsz) * 0.5 ), ty15 ), 0x66FFFFFF, range );
}

void PrintSource( const std::vector<Tokenizer::Line>& lines )
{
    for( auto& line: lines )
    {
        auto ptr = line.begin;
        auto it = line.tokens.begin();
        while( ptr < line.end )
        {
            if( it == line.tokens.end() )
            {
                ImGui::TextUnformatted( ptr, line.end );
                ImGui::SameLine( 0, 0 );
                break;
            }
            if( ptr < it->begin )
            {
                ImGui::TextUnformatted( ptr, it->begin );
                ImGui::SameLine( 0, 0 );
            }
            auto color = SyntaxColors[(int)it->color];
            TextColoredUnformatted( color, it->begin, it->end );
            ImGui::SameLine( 0, 0 );
            ptr = it->end;
            ++it;
        }
        ImGui::ItemSize( ImVec2( 0, 0 ), 0 );
    }
}

}
