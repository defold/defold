package com.dynamo.cr.server.resources;

import java.util.List;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.GET;
import javax.ws.rs.Path;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.protocol.proto.Protocol.ProductInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProductInfoList;
import com.dynamo.cr.protocol.proto.Protocol.ProductInfoList.Builder;
import com.dynamo.cr.server.model.Product;

@Path("/products")
@RolesAllowed(value = { "user" })
public class ProductsResource extends BaseResource {

    protected static Logger logger = LoggerFactory.getLogger(ProductsResource.class);

    @GET
    public ProductInfoList getProducts() {
        List<Product> list = em.createQuery("select p from Product p", Product.class)
                .getResultList();

        Builder listBuilder = ProductInfoList.newBuilder();
        for (Product product : list) {
            ProductInfo pi = ResourceUtil.createProductInfo(product);
            listBuilder.addProducts(pi);
        }

        return listBuilder.build();
    }

}
