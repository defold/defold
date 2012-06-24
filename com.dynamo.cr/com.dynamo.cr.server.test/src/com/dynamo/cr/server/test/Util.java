package com.dynamo.cr.server.test;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.ArrayList;

public class Util {

    public static void dropAllTables() throws ClassNotFoundException, SQLException {
        Connection con = DriverManager.getConnection("jdbc:derby:tmp/testdb");
        ResultSet rs = con.getMetaData().getTables(null, null, null, new String[] {"TABLE"});
        ArrayList<String> queries = new ArrayList<String>();
        while (rs.next()) {
            String q = "DROP TABLE " + rs.getString("TABLE_SCHEM") + "." + rs.getString("TABLE_NAME");
            queries.add(q);
        }

        // Brute force drop of all tables. We don't know the correct drop order due to constraints
        // iterate until done
        int iter = 0;
        while (queries.size() > 0) {
            String q = queries.remove(0);
            Statement stmnt = con.createStatement();
            try {
                stmnt.execute(q);
            }
            catch (SQLException e) {
                // Failed to drop. Add last in query list
                queries.add(q);
            }
            finally {
                stmnt.close();
            }
            ++iter;
            if (iter > 100) {
                throw new RuntimeException(String.format("Unable to drop all tables after %d iterations. Something went very wrong", iter));
            }
        }
        con.close();
    }

    public static void clearAllTables() throws ClassNotFoundException, SQLException {
        Connection con = DriverManager.getConnection("jdbc:derby:tmp/testdb");
        ResultSet rs = con.getMetaData().getTables(null, null, null, new String[] {"TABLE"});
        ArrayList<String> queries = new ArrayList<String>();
        while (rs.next()) {
            String q = "DELETE FROM " + rs.getString("TABLE_SCHEM") + "." + rs.getString("TABLE_NAME");
            queries.add(q);
        }

        // Brute force delete of all rows. We don't know the correct order due to constraints
        // iterate until done
        int iter = 0;
        while (queries.size() > 0) {
            String q = queries.remove(0);
            Statement stmnt = con.createStatement();
            try {
                stmnt.execute(q);
            }
            catch (SQLException e) {
                // Failed to drop. Add last in query list
                queries.add(q);
            }
            finally {
                stmnt.close();
            }
            if (iter > 100) {
                throw new RuntimeException(String.format("Unable to clear all tables after %d iterations. Something went very wrong", iter));
            }
        }
        con.close();
    }

}
