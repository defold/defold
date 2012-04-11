package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.PlaceTokenizer;
import com.google.gwt.place.shared.Prefix;

public class ProductInfoPlace extends DefoldPlace {

    @Override
    public String getTitle() {
        return "Welcome";
    }

    @Prefix("!product_info")
    public static class Tokenizer implements PlaceTokenizer<ProductInfoPlace> {
        @Override
        public String getToken(ProductInfoPlace place) {
            return "";
        }

        @Override
        public ProductInfoPlace getPlace(String token) {
            return new ProductInfoPlace();
        }
    }
}
