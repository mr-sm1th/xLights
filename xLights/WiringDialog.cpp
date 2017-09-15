#include "WiringDialog.h"

//(*InternalHeaders(WiringDialog)
#include <wx/intl.h>
#include <wx/string.h>
//*)

#include <map>
#include <list>
#include <wx/position.h>
#include <wx/dcmemory.h>
#include <wx/dcscreen.h>
#include <wx/image.h>
#include <wx/menu.h>
#include <wx/filepicker.h>
#include <wx/config.h>
#include "models/Model.h"

#define MINFONTSIZE 8
#define FONTSIZEINCREMENT 4

//(*IdInit(WiringDialog)
const long WiringDialog::ID_STATICBITMAP1 = wxNewId();
//*)

const long WiringDialog::ID_MNU_EXPORT = wxNewId();
const long WiringDialog::ID_MNU_DARK = wxNewId();
const long WiringDialog::ID_MNU_LIGHT = wxNewId();
const long WiringDialog::ID_MNU_FRONT = wxNewId();
const long WiringDialog::ID_MNU_REAR = wxNewId();
const long WiringDialog::ID_MNU_FONTSMALLER = wxNewId();
const long WiringDialog::ID_MNU_FONTLARGER = wxNewId();

BEGIN_EVENT_TABLE(WiringDialog,wxDialog)
	//(*EventTable(WiringDialog)
	//*)
END_EVENT_TABLE()

WiringDialog::WiringDialog(wxWindow* parent, wxString modelname, wxWindowID id,const wxPoint& pos,const wxSize& size)
{
    _dark = true;
    _rear = true;
    _multilight = false;
    _cols = 1;
    _rows = 1;

	//(*Initialize(WiringDialog)
	wxFlexGridSizer* FlexGridSizer1;

	Create(parent, id, _("Custom Model Wiring"), wxDefaultPosition, wxDefaultSize, wxCAPTION|wxRESIZE_BORDER|wxCLOSE_BOX|wxMAXIMIZE_BOX, _T("id"));
	SetClientSize(wxSize(500,500));
	Move(wxDefaultPosition);
	SetMinSize(wxSize(500,500));
	FlexGridSizer1 = new wxFlexGridSizer(0, 1, 0, 0);
	FlexGridSizer1->AddGrowableCol(0);
	FlexGridSizer1->AddGrowableRow(0);
	StaticBitmap_Wiring = new wxStaticBitmap(this, ID_STATICBITMAP1, wxNullBitmap, wxDefaultPosition, wxSize(500,500), wxSIMPLE_BORDER, _T("ID_STATICBITMAP1"));
	FlexGridSizer1->Add(StaticBitmap_Wiring, 1, wxALL|wxEXPAND|wxFIXED_MINSIZE, 5);
	SetSizer(FlexGridSizer1);
	SetSizer(FlexGridSizer1);
	Layout();

	Connect(wxEVT_SIZE,(wxObjectEventFunction)&WiringDialog::OnResize);
	//*)

    Connect(ID_STATICBITMAP1, wxEVT_CONTEXT_MENU, (wxObjectEventFunction)&WiringDialog::RightClick);

    _modelname = modelname;

    wxConfigBase* config = wxConfigBase::Get();
    config->Read("xLightsWDFontSize", &_fontSize, 12);
}

void WiringDialog::SetData(Model* model)
{
    int nodes = model->GetNodeCount();

    std::vector<NodeBaseClassPtr> nodeList;
    model->InitRenderBufferNodes("Per Preview", "None", nodeList, _cols, _rows);

    float minx = 999999;
    float miny = 999999;
    float maxx = 0;
    float maxy = 0;
    for (int i = 0; i < nodes; ++i)
    {
        auto points = nodeList[i]->Coords;

        for (auto it = points.begin(); it != points.end(); ++it)
        {
            float x = it->screenX;
            float y = it->screenY;

            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        }
    }

    // The 4 rather than 1 adds some extra space
    _cols = maxx - minx + 8;
    _rows = maxy - miny + 4;

    int string = 0;
    int stringnode = 1;
    std::map<int, std::list<wxPoint>> data;
    for (int i = 0; i < nodes; ++i)
    {
        if (model->GetNodeStringNumber(i) != string && model->GetDisplayAs() != "Custom")
        {
            _points[string] = data;
            data.clear();
            stringnode = 1;
            string = model->GetNodeStringNumber(i);
        }
        auto points = nodeList[i]->Coords;
        for (auto it = points.begin(); it != points.end(); ++it)
        {
            float x = it->screenX;
            float y = it->screenY;
            x = x - minx;
            y = y - miny + 2;
            if (model->GetDisplayAs() != "Icicles")
            {
                y = _rows - y;
            }
            wxASSERT(x >= 0 && x < _cols);
            wxASSERT(y >= 0 && y <= _rows);
            data[stringnode].push_back(wxPoint(x, y));
        }
        stringnode++;
    }
    _points[string] = data;
}

void WiringDialog::SetData(wxGrid* grid, bool reverse)
{
    _cols = grid->GetNumberCols();
    _rows = grid->GetNumberRows();

    _points[0] = ExtractPoints(grid, reverse);

    _multilight = false;
    for (auto itp = _points.begin(); itp != _points.end(); ++itp)
    {
        for (auto it = itp->second.begin(); it != itp->second.end(); ++it)
        {
            if (it->second.size() > 1)
            {
                _multilight = true;
                break;
            }
        }
    }

    Render();
}

void RenderText(const wxString& text, wxMemoryDC& dc, int x, int y, wxColor fore, wxColor back)
{
    dc.SetTextForeground(back);
    dc.DrawText(text, x - 1, y);
    dc.DrawText(text, x+1, y);
    dc.DrawText(text, x, y-1);
    dc.DrawText(text, x, y+1);

    dc.SetTextForeground(fore);
    dc.DrawText(text, x, y);
}

void WiringDialog::RenderNodes(std::map<int, std::map<int, std::list<wxPoint>>>& points, int width, int height)
{
    wxMemoryDC dc;
    dc.SelectObject(_bmp);

    if (_dark)
    {
        dc.SetPen(*wxBLACK_PEN);
        dc.SetBrush(*wxBLACK_BRUSH);
    }
    else
    {
        dc.SetPen(*wxWHITE_PEN);
        dc.SetBrush(*wxWHITE_BRUSH);
    }
    dc.DrawRectangle(wxPoint(0, 0), _bmp.GetScaledSize());

    int r = 0.6 * std::min(_bmp.GetScaledWidth() / width / 2, _bmp.GetScaledHeight() / height / 2);
    if (r == 0) r = 1;
    if (r > 5) r = 5;

    wxFont font(_fontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString, wxFONTENCODING_DEFAULT);
    dc.SetFont(font);

    wxPen* pen;
    if (_dark)
    {
        pen = new wxPen(*wxYELLOW, 2);
    }
    else
    {
        pen = new wxPen(*wxBLACK, 2);
    }

    // draw the lines
    for (auto itp = points.begin(); itp != points.end(); ++itp)
    {
        int last = -10;
        wxPoint lastpt = wxPoint(0, 0);

        for (auto it = itp->second.begin(); it != itp->second.end(); ++it)
        {
            dc.SetBrush(*wxWHITE_BRUSH);
            dc.SetPen(*wxBLACK_PEN);
            int x = (width - it->second.front().x) * (_bmp.GetScaledWidth() - 40) / width - 20;
            if (!_rear)
            {
                x = _bmp.GetScaledWidth() - x - 40;
            }
            int y = 20 + it->second.front().y * (_bmp.GetScaledHeight() - 40) / height;

            if (it->first == last + 1)
            {
                dc.SetPen(*pen);
                int lastx = (width - lastpt.x) * (_bmp.GetScaledWidth() - 40) / width - 20;
                if (!_rear)
                {
                    lastx = _bmp.GetScaledWidth() - lastx - 40;
                }
                int lasty = 20 + lastpt.y * (_bmp.GetScaledHeight() - 40) / height;
                dc.DrawLine(lastx, lasty, x, y);
            }

            last = it->first;
            lastpt = it->second.front();
        }
    }

    // now the circles
    for (auto itp = points.begin(); itp != points.end(); ++itp)
    {
        for (auto it = itp->second.begin(); it != itp->second.end(); ++it)
        {
            int x = (width - it->second.front().x) * (_bmp.GetScaledWidth() - 40) / width - 20;
            if (!_rear)
            {
                x = _bmp.GetScaledWidth() - x - 40;
            }
            int y = 20 + it->second.front().y * (_bmp.GetScaledHeight() - 40) / height;
            dc.DrawCircle(x, y, r);
        }
    }

    // render the text after the lines so the text is not drawn over
    int string = 1;
    for (auto itp = points.begin(); itp != points.end(); ++itp)
    {
        for (auto it = itp->second.begin(); it != itp->second.end(); ++it)
        {
            int x = (width - it->second.front().x) * (_bmp.GetScaledWidth() - 40) / width - 20;
            if (!_rear)
            {
                x = _bmp.GetScaledWidth() - x - 40;
            }
            int y = 20 + it->second.front().y * (_bmp.GetScaledHeight() - 40) / height;

            std::string label;
            if(points.size() == 1)
            {
                label = wxString::Format("%d", it->first).ToStdString();
            }
            else
            {
                label = wxString::Format("%d:%d", string, it->first).ToStdString();
            }

            if (_dark)
            {
                RenderText(label, dc, x + r + 2, y, *wxLIGHT_GREY, *wxBLACK);
            }
            else
            {
                RenderText(label, dc, x + r + 2, y, *wxBLACK, *wxWHITE);
            }
        }
        string++;
    }

    if (_dark)
    {
        if (_rear)
        {
            RenderText("CAUTION: Reverse view", dc, 20, 20, *wxGREEN, *wxBLACK);
        }
        else
        {
            RenderText("CAUTION: Front view", dc, 20, 20, *wxBLUE, *wxBLACK);
        }
        RenderText("Model: " + _modelname, dc, 20, 20 + _fontSize + 4, *wxGREEN, *wxBLACK);
    }
    else
    {
        if (_rear)
        {
            RenderText("CAUTION: Reverse view", dc, 20, 20, *wxBLACK, *wxWHITE);
        }
        else
        {
            RenderText("CAUTION: Front view", dc, 20, 20, *wxBLUE, *wxWHITE);
        }
        RenderText("Model: " + _modelname, dc, 20, 20 + _fontSize + 4, *wxBLACK, *wxWHITE);
    }

    dc.SetPen(*wxBLACK_PEN);
    delete pen;
}

void WiringDialog::RenderMultiLight(std::map<int, std::map<int, std::list<wxPoint>>>& points, int width, int height)
{
    static wxColor magenta(255, 0, 255);
    static const wxColor* colors[] = { wxRED, wxBLUE, wxGREEN, wxYELLOW, wxLIGHT_GREY, wxCYAN, wxWHITE, &magenta };
    static const wxColor* lightColors[] = { wxRED, wxBLUE, wxGREEN, wxYELLOW, wxLIGHT_GREY, wxCYAN, wxBLACK, &magenta };
    static int colorcnt = sizeof(colors) / sizeof(wxColor*);
    wxMemoryDC dc;
    dc.SelectObject(_bmp);

    if (_dark)
    {
        dc.SetPen(*wxBLACK_PEN);
        dc.SetBrush(*wxBLACK_BRUSH);
    }
    else
    {
        dc.SetPen(*wxWHITE_PEN);
        dc.SetBrush(*wxWHITE_BRUSH);
    }
    dc.DrawRectangle(wxPoint(0, 0), _bmp.GetScaledSize());

    wxFont font(_fontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString, wxFONTENCODING_DEFAULT);
    dc.SetFont(font);

    int cindex = 0;

    int r = 0.8 * std::min(_bmp.GetScaledWidth() / width / 2, _bmp.GetScaledHeight() / height / 2);
    if (r == 0) r = 1;
    if (r > 5) r = 5;

    for (auto itp = points.begin(); itp != points.end(); ++itp)
    {
        for (auto it = itp->second.begin(); it != itp->second.end(); ++it)
        {
            if (_dark)
            {
                dc.SetBrush(wxBrush(*colors[cindex], wxBRUSHSTYLE_SOLID));
            }
            else
            {
                dc.SetBrush(wxBrush(*lightColors[cindex], wxBRUSHSTYLE_SOLID));
            }

            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
            {
                int x = (width - it2->x) * (_bmp.GetScaledWidth() - 40) / width - 20;
                if (!_rear)
                {
                    x = _bmp.GetScaledWidth() - x - 40;
                }
                int y = 20 + it2->y * (_bmp.GetScaledHeight() - 40) / height;
                dc.DrawCircle(x, y, r);
            }

            cindex++;
            cindex %= colorcnt;
        }
    }

    int string = 1;
    for (auto itp = points.begin(); itp != points.end(); ++itp)
    {
        for (auto it = itp->second.begin(); it != itp->second.end(); ++it)
        {
            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
            {
                int x = (width - it2->x) * (_bmp.GetScaledWidth() - 40) / width - 20;
                if (!_rear)
                {
                    x = _bmp.GetScaledWidth() - x - 40;
                }
                int y = 20 + it2->y * (_bmp.GetScaledHeight() - 40) / height;

                std::string label;
                if (points.size() == 1)
                {
                    label = wxString::Format("%d", it->first).ToStdString();
                }
                else
                {
                    label = wxString::Format("%d:%d", string, it->first).ToStdString();
                }

                if (_dark)
                {
                    RenderText(label, dc, x + r + 2, y, *wxLIGHT_GREY, *wxBLACK);
                }
                else
                {
                    RenderText(label, dc, x + r + 2, y, *wxBLACK, *wxWHITE);
                }
            }
        }
        string++;
    }

    if (_dark)
    {
        if (_rear)
        {
            RenderText("CAUTION: Reverse view", dc, 20, 20, *wxGREEN, *wxBLACK);
        }
        else
        {
            RenderText("CAUTION: Front view", dc, 20, 20, *wxBLUE, *wxBLACK);
        }
        RenderText("Model: " + _modelname, dc, 20, 20 + _fontSize + 4, *wxGREEN, *wxBLACK);
    }
    else
    {
        if (_rear)
        {
            RenderText("CAUTION: Reverse view", dc, 20, 20, *wxBLACK, *wxWHITE);
        }
        else
        {
            RenderText("CAUTION: Front view", dc, 20, 20, *wxBLUE, *wxWHITE);
        }
        RenderText("Model: " + _modelname, dc, 20, 20 + _fontSize + 4, *wxBLACK, *wxWHITE);
    }
}

std::map<int, std::list<wxPoint>> WiringDialog::ExtractPoints(wxGrid* grid, bool reverse)
{
    std::map<int, std::list<wxPoint>> res;

    for (size_t r = 0; r < grid->GetNumberRows(); r++)
    {
        if (reverse)
        {
            for (int c = grid->GetNumberCols() - 1; c >= 0; c--)
            {
                wxString val = grid->GetCellValue(r, grid->GetNumberCols() - 1 - c);
                if (val != "")
                {
                    res[wxAtoi(val)].push_back(wxPoint(c, r));
                }
            }
        }
        else
        {
            for (int c = 0; c < grid->GetNumberCols(); c++)
            {
                wxString val = grid->GetCellValue(r, c);
                if (val != "")
                {
                    res[wxAtoi(val)].push_back(wxPoint(c, r));
                }
            }
        }
    }

    return res;
}

WiringDialog::~WiringDialog()
{
	//(*Destroy(WiringDialog)
	//*)
}

void WiringDialog::OnResize(wxSizeEvent& event)
{
    Render();
    event.Skip();
}

void WiringDialog::RightClick(wxContextMenuEvent& event)
{
    wxMenu mnuLayer;
    mnuLayer.Append(ID_MNU_EXPORT, "Export");
    auto dark = mnuLayer.Append(ID_MNU_DARK, "Dark", "", wxITEM_RADIO);
    auto light = mnuLayer.Append(ID_MNU_LIGHT, "Light", "", wxITEM_RADIO);
    if (_dark)
    {
        dark->Check();
    }
    else
    {
        light->Check();
    }
    auto fontSmaller = mnuLayer.Append(ID_MNU_FONTSMALLER, "Smaller Font");
    if (_fontSize <= MINFONTSIZE) fontSmaller->Enable(false);
    mnuLayer.Append(ID_MNU_FONTLARGER, "Larger Font");
    auto front = mnuLayer.Append(ID_MNU_FRONT, "Front", "", wxITEM_RADIO);
    auto rear = mnuLayer.Append(ID_MNU_REAR, "Rear", "", wxITEM_RADIO);
    if (_rear)
    {
        rear->Check();
    }
    else
    {
        front->Check();
    }
    mnuLayer.Connect(wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&WiringDialog::OnPopup, nullptr, this);
    PopupMenu(&mnuLayer);
}

void WiringDialog::OnPopup(wxCommandEvent& event)
{
    int id = event.GetId();
    if (id == ID_MNU_EXPORT)
    {
        wxString filename = wxFileSelector(_("Choose output file"), wxEmptyString, _modelname, wxEmptyString, "PNG File (*.png)|*.png", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (filename != "")
        {
            wxImage img = _bmp.ConvertToImage();
            img.SaveFile(filename, wxBITMAP_TYPE_PNG);
        }
    }
    else if (id == ID_MNU_DARK)
    {
        _dark = true;
        Render();
    }
    else if (id == ID_MNU_LIGHT)
    {
        _dark = false;
        Render();
    }
    else if (id == ID_MNU_FRONT)
    {
        _rear = false;
        Render();
    }
    else if (id == ID_MNU_REAR)
    {
        _rear = true;
        Render();
    }
    else if (id == ID_MNU_FONTLARGER)
    {
        _fontSize += FONTSIZEINCREMENT;
        wxConfigBase* config = wxConfigBase::Get();
        config->Write(_("xLightsWDFontSize"), _fontSize);
        Render();
    }
    else if (id == ID_MNU_FONTSMALLER)
    {
        _fontSize -= FONTSIZEINCREMENT;
        if (_fontSize < MINFONTSIZE) _fontSize = MINFONTSIZE;
        wxConfigBase* config = wxConfigBase::Get();
        config->Write(_("xLightsWDFontSize"), _fontSize);
        Render();
    }
}

void WiringDialog::Render()
{
    //wxScreenDC sdc;
    //wxSize s = sdc.GetSize();
    //_bmp.CreateScaled(s.GetWidth(), s.GetHeight(), wxBITMAP_SCREEN_DEPTH, GetContentScaleFactor());

    int w, h;
    GetClientSize(&w, &h);
    _bmp.CreateScaled(w, h, wxBITMAP_SCREEN_DEPTH, GetContentScaleFactor());

    if (_multilight)
    {
        RenderMultiLight(_points, _cols, _rows);
    }
    else
    {
        RenderNodes(_points, _cols, _rows);
    }

    StaticBitmap_Wiring->SetBitmap(_bmp);
    Refresh();
}