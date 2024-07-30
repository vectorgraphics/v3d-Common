## A common set of files used to support V3D files inside of Okular Plugins.

# To use the plugin:
1. Add an instance of `V3dModelManager` as a member of your generator
2. In the `loadDocument` function of your generator give the model manager a pointer to the document, and add in any models located in the document:
    ```
    if (document() != nullptr) {
        m_ModelManager.SetDocument(document());
    }

    m_ModelManager.AddModel(V3dModel{ fileName.toStdString() }, 0);
    ```
3. In the `generatPixmap` function of you generator, cache the size of the request, than call the Render function and present the QImage to the page however you want to:
    ```    
    m_ModelManager.CacheRequestSize(request->page()->number(), request->width(), request->height(), request->priority());

    QImage image = m_ModelManager.RenderModel(0, 0, (int)request->width(), (int)request->height());
    ```