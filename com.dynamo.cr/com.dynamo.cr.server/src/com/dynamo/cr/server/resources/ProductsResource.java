package com.dynamo.cr.server.resources;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.QueryParam;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.protocol.proto.Protocol.ProductInfoList;
import com.dynamo.cr.protocol.proto.Protocol.ProductInfoList.Builder;

@Path("/products")
@RolesAllowed(value = { "user" })
public class ProductsResource extends BaseResource {

    protected static Logger logger = LoggerFactory.getLogger(ProductsResource.class);

    @GET
    public ProductInfoList getProducts(@QueryParam("handle") String handle) {
        String billingApiUrl = server.getConfiguration().getBillingApiUrl();
        Builder listBuilder = ProductInfoList.newBuilder();
        if (handle != null) {
            listBuilder.addProducts(ResourceUtil.createProductInfo(server.getProductByHandle(handle), billingApiUrl));
        } else {
            // Return every
            for (BillingProduct product : server.getConfiguration().getProductsList()) {
                listBuilder.addProducts(ResourceUtil.createProductInfo(product, billingApiUrl));
            }
        }
        return listBuilder.build();
    }

}
