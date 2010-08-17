#pragma once

// Graph Builder Callback: 
//
//  Used to verify the filters that are selected for possible use in 
//  a DirectShow filter graph during automatic graph building.
//
//  Usage:
//
//  	CGraphBuilderCallback graph_builder_callback(NULL);
//
//     CComQIPtr<IObjectWithSite> object_with_site(m_dx->graph_builder);
//      if (object_with_site)
//         HRX(object_with_site->SetSite( (IAMGraphBuilderCallback*) &graph_builder_callback ));
//
//		... build and render filter graph with RenderFile or rendering a particular pin
//     ... the callback will be invoked for each filter selected by intelligent connect for possible inclusion in the graph
//     ... Example:  HRX(m_dx->graph_builder->Render(pin));
// 	
//     if (object_with_site)
//          hr = object_with_site->SetSite(NULL);

class CGraphBuilderCallback : public CUnknown,
   public IAMGraphBuilderCallback
{
public:
     CGraphBuilderCallback(HWND window);
    ~CGraphBuilderCallback();

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID, void**);
    // IAMGraphBuilderCallback
    STDMETHODIMP SelectedFilter(IMoniker *moniker);
    STDMETHODIMP CreatedFilter(IBaseFilter *filter);

};


