package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;
import com.google.gwt.place.shared.PlaceTokenizer;

public class ProductInfoPlace extends Place {
    public static class Tokenizer implements PlaceTokenizer<ProductInfoPlace> {
        @Override
        public String getToken(ProductInfoPlace place) {
            return "product_info";
        }

        @Override
        public ProductInfoPlace getPlace(String token) {
            return new ProductInfoPlace();
        }
    }
}
